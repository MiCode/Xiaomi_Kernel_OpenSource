// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/sched.h>
#include "mtk_drm_panel_drv.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define MAX_PANEL_OPERATION_NAME (256)
#define MTK_DRM_PANEL_DEBUG

static struct mtk_panel_context *ctx_dsi;
static unsigned int mtk_lcm_support_cb;

struct mtk_panel_context *panel_to_lcm(
		struct drm_panel *panel)
{
	return container_of(panel, struct mtk_panel_context, panel);
}
EXPORT_SYMBOL(panel_to_lcm);

static int mtk_drm_lcm_dsi_init_ctx(void)
{
	if (IS_ERR_OR_NULL(ctx_dsi)) {
		LCM_KZALLOC(ctx_dsi,
			sizeof(struct mtk_panel_context), GFP_KERNEL);
		if (ctx_dsi == NULL) {
			DDPPR_ERR("%s, %d, failed to allocate ctx\n", __func__, __LINE__);
			return -ENOMEM;
		}
	}

	if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		LCM_KZALLOC(ctx_dsi->panel_resource,
			sizeof(struct mtk_panel_resource), GFP_KERNEL);
		if (ctx_dsi->panel_resource == NULL) {
			DDPPR_ERR("%s: failed to allocate panel resource\n", __func__);
			return -ENOMEM;
		}
	}

	return 0;
}

static void mtk_drm_lcm_dsi_deinit_ctx(void)
{
	if (ctx_dsi == NULL)
		return;

	if (ctx_dsi->panel_resource != NULL) {
		free_lcm_resource(MTK_LCM_FUNC_DSI, ctx_dsi->panel_resource);
		ctx_dsi->panel_resource = NULL;
	}
	LCM_KFREE(ctx_dsi, sizeof(struct mtk_panel_context));
	DDPMSG("%s,%d, free panel resource, total_size:%lluByte\n",
		__func__, __LINE__, mtk_lcm_total_size);
}

int mtk_panel_dsi_dcs_write_buffer(struct mipi_dsi_device *dsi_dev,
		const void *data, size_t len)
{
	ssize_t ret;
	char *addr;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi_dev, data, len);
	else
		ret = mipi_dsi_generic_write(dsi_dev, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "error %zd writing seq: %ph\n",
			ret, data);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_write_buffer);

int mtk_panel_dsi_dcs_write(struct mipi_dsi_device *dsi_dev,
		u8 cmd, void *data, size_t len)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_dcs_write(dsi_dev, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "error %d write dcs cmd:(%#x)\n",
			ret, cmd);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_write);

int mtk_panel_dsi_dcs_read(struct mipi_dsi_device *dsi_dev,
		u8 cmd, void *data, size_t len)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi_dev, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev,
			"error %d reading dcs cmd:(0x%x)\n", ret, cmd);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_read);

int mtk_panel_dsi_dcs_read_buffer(struct mipi_dsi_device *dsi_dev,
		const void *data_in, size_t len_in,
		void *data_out, size_t len_out)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_generic_read(dsi_dev, data_in,
			len_in, data_out, len_out);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "error %d reading buffer seq:(%p)\n",
			ret, data_in);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_read_buffer);

/* register customization callback of panel operation */
int mtk_panel_register_dsi_customization_funcs(
		const struct mtk_panel_cust *cust)
{
	int ret = mtk_drm_lcm_dsi_init_ctx();

	if (ret < 0) {
		DDPPR_ERR("%s, invalid ctx dsi\n", __func__);
		return ret;
	}

	if (ctx_dsi->panel_resource->cust != NULL) {
		DDPPR_ERR("%s %d: cust callback has already been registered\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	ctx_dsi->panel_resource->cust = cust;
	DDPMSG("%s--\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_panel_register_dsi_customization_funcs);

int mtk_panel_deregister_dsi_customization_funcs(
		const struct mtk_panel_cust *cust)
{
	DDPMSG("%s ++\n", __func__);
	if (ctx_dsi->panel_resource == NULL ||
		ctx_dsi->panel_resource->cust == NULL) {
		DDPMSG("%s %d: cust callback has already been un-registered\n",
			__func__, __LINE__);
		return 0;
	}

	if (ctx_dsi->panel_resource->cust == cust)
		ctx_dsi->panel_resource->cust = NULL;
	else {
		DDPPR_ERR("%s invalid cust ops\n", __func__);
		return -EFAULT;
	}

	DDPMSG("%s--\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_panel_deregister_dsi_customization_funcs);

static int mtk_drm_panel_unprepare(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_params *params = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	const struct mtk_panel_cust *cust = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (atomic_read(&ctx_dsi->prepared) == 0) {
		DDPMSG("%s, no need to unprepare\n", __func__);
		return 0;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.unprepare != NULL) {
		DDPMSG("%s, %d cust unprepare\n", __func__, __LINE__);
		ret = cust->funcs.unprepare(panel);
		if (ret < 0) {
			DDPPR_ERR("%s,%d: failed to do panel unprepare, %d\n",
				__func__, __LINE__, ret);
			return ret;
		}
		goto end;
	}

	params = &ctx_dsi->panel_resource->params;
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(params) ||
		IS_ERR_OR_NULL(ops))
		return -EINVAL;

	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->unprepare, ctx_dsi->panel_resource,
			NULL, NULL, NULL, prop, "panel_unprepare");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel unprepare, %d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	ret = mtk_drm_gateic_power_off(MTK_LCM_FUNC_DSI);
	if (ret < 0)
		DDPPR_ERR("%s, gate ic power off failed, %d\n",
			__func__, ret);

end:
	atomic_set(&ctx_dsi->error, 0);
	atomic_set(&ctx_dsi->prepared, 0);
	atomic_set(&ctx_dsi->hbm_en, 0);
	DDPMSG("%s- %d\n", __func__, ret);
	return ret;
}

static int mtk_drm_panel_do_prepare(struct mtk_panel_context *ctx_dsi)
{
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_input_packet input;
	unsigned long flags = 0;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	unsigned long long start = 0, end = 0;

	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	DDPMSG("%s, %d, prop:0x%x size:%lluByte\n",
		__func__, __LINE__, prop, mtk_lcm_total_size);
	start = sched_clock();
	if (mtk_lcm_create_input_packet(&input, 1, 1) < 0)
		return -ENOMEM;

	if (mtk_lcm_create_input(input.condition, 1,
			MTK_LCM_INPUT_TYPE_CURRENT_MODE) < 0)
		goto fail2;
	spin_lock_irqsave(&ctx_dsi->lock, flags);
	*(unsigned int *)input.condition->data = ctx_dsi->current_mode->id;
	spin_unlock_irqrestore(&ctx_dsi->lock, flags);

	if (mtk_lcm_create_input(input.data, 1,
			MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0)
		goto fail1;
	*(u8 *)input.data->data = atomic_read(&ctx_dsi->current_backlight);

	/*do panel initialization*/
	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->prepare, ctx_dsi->panel_resource,
			&input, NULL, NULL, prop, "panel_prepare");

	if (ret < 0)
		DDPPR_ERR("%s,%d: failed to do panel prepare\n",
			__func__, __LINE__);

	/*deallocate input data*/
	mtk_lcm_destroy_input(input.data);
fail1:
	mtk_lcm_destroy_input(input.condition);
fail2:
	mtk_lcm_destroy_input_packet(&input);
	end = sched_clock();

	DDPMSG("%s, %d, prop:0x%x time:%lluns, ret:%d, size:%lluByte\n",
		__func__, __LINE__, prop, end - start, ret, mtk_lcm_total_size);
	return ret;
}

static int mtk_drm_panel_prepare(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	const struct mtk_panel_cust *cust = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (atomic_read(&ctx_dsi->prepared) != 0) {
		DDPMSG("%s, no need to prepare\n", __func__);
		return 0;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.prepare != NULL) {
		ret = cust->funcs.prepare(panel);
		if (ret < 0) {
			DDPPR_ERR("%s,%d: failed to do panel prepare, %d\n",
				__func__, __LINE__, ret);
			return ret;
		}
		goto end;
	}

	ret = mtk_drm_gateic_power_on(MTK_LCM_FUNC_DSI);
	if (ret < 0) {
		DDPPR_ERR("%s, gate ic power on failed, %d\n",
			__func__, ret);
		return ret;
	}

	if (ctx_dsi->current_mode->voltage != 0) {
		ret = mtk_drm_gateic_set_voltage(
				ctx_dsi->current_mode->voltage,
				MTK_LCM_FUNC_DSI);
		if (ret != 0) {
			DDPPR_ERR("%s, gate ic set voltage:%u failed, %d\n",
				__func__, ctx_dsi->current_mode->voltage, ret);
			return ret;
		}
	}

	ret = mtk_drm_panel_do_prepare(ctx_dsi);
	if (ret != 0 ||
		atomic_read(&ctx_dsi->error) < 0) {
		mtk_drm_panel_unprepare(panel);
		return -1;
	}
	mtk_panel_tch_rst(panel);

end:
	atomic_set(&ctx_dsi->prepared, 1);
	DDPMSG("%s-, %d\n", __func__, ret);
	return ret;
}

static int mtk_drm_panel_enable(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	const struct mtk_panel_cust *cust = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (atomic_read(&ctx_dsi->enabled) != 0)
		return 0;

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.enable != NULL) {
		ret = cust->funcs.enable(panel);
		if (ret < 0) {
			DDPPR_ERR("%s,%d: failed to do panel enable\n",
				__func__, __LINE__);
			return ret;
		}

		goto end;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->enable, ctx_dsi->panel_resource,
			NULL, NULL, NULL, prop, "panel_enable");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel enable\n",
			__func__, __LINE__);
		return ret;
	}

	if (ctx_dsi->backlight) {
		ctx_dsi->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx_dsi->backlight);
	}

end:
	atomic_set(&ctx_dsi->enabled, 1);
	DDPMSG("%s-, %d\n", __func__, ret);
	return 0;
}

static int mtk_drm_panel_disable(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	const struct mtk_panel_cust *cust = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (atomic_read(&ctx_dsi->enabled) == 0)
		return 0;

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.disable != NULL) {
		ret = cust->funcs.disable(panel);
		if (ret < 0) {
			DDPPR_ERR("%s,%d: failed to do panel disable\n",
				__func__, __LINE__);
			return ret;
		}
		goto end;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	if (ctx_dsi->backlight) {
		ctx_dsi->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx_dsi->backlight);
	}

	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->disable, ctx_dsi->panel_resource,
			NULL, NULL, NULL, prop, "panel_disable");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel disable\n",
			__func__, __LINE__);
		return ret;
	}

end:
	atomic_set(&ctx_dsi->enabled, 0);
	DDPMSG("%s- %d\n", __func__, ret);
	return 0;
}

static struct drm_display_mode *get_mode_by_connector_id(
	struct drm_connector *connector, unsigned int id)
{
	struct drm_display_mode *mode;
	unsigned int i = 0;

	list_for_each_entry(mode, &connector->modes, head) {
		if (i == id)
			return mode;
		i++;
	}
	return NULL;
}

static int mtk_panel_ext_param_set(struct drm_panel *panel,
			struct drm_connector *connector, unsigned int id)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	struct mtk_lcm_params_dsi *params = NULL;
	struct mtk_lcm_mode_dsi *mode_node;
	unsigned long flags = 0;
	bool found = false;
	struct drm_display_mode *mode = NULL;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPINFO("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.ext_param_set != NULL)
		return cust->ext_funcs.ext_param_set(panel,
				connector, id);

	mode = get_mode_by_connector_id(connector, id);
	if (IS_ERR_OR_NULL(mode)) {
		DDPMSG("%s, failed to get mode\n", __func__);
		return -EINVAL;
	}

	if (ext == NULL) {
		DDPMSG("%s, failed to get ext\n", __func__);
		return -EINVAL;
	}

	params = &ctx_dsi->panel_resource->params.dsi_params;
	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
		params->mode_count == 0)
		return -EINVAL;


	list_for_each_entry(mode_node, &params->mode_list, list) {
		if (drm_mode_equal(&mode_node->mode, mode) == true) {
			found = true;
			break;
		}
	}

	if (found == false) {
		DDPPR_ERR("%s: invalid id:%u\n", __func__, id);
		return -EINVAL;
	}
	ext->params = &mode_node->ext_param;
	spin_lock_irqsave(&ctx_dsi->lock, flags);
	ctx_dsi->current_mode = mode_node;
	spin_unlock_irqrestore(&ctx_dsi->lock, flags);

	DDPMSG("%s-, id:%u, fps:%u\n", __func__,
		id, ctx_dsi->current_mode->fps);
	return 0;
}

static int mtk_panel_ext_param_get(struct drm_panel *panel,
		struct drm_connector *connector,
		struct mtk_panel_params **ext_param,
		unsigned int id)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	struct mtk_lcm_params_dsi *params = NULL;
	bool found = false;
	struct drm_display_mode *mode = NULL;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.ext_param_get != NULL)
		return cust->ext_funcs.ext_param_get(panel,
				connector, ext_param, id);

	mode = get_mode_by_connector_id(connector, id);
	if (IS_ERR_OR_NULL(mode)) {
		DDPMSG("%s, failed to get mode\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ext_param) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx, resource\n", __func__);
		return -EINVAL;
	}

	params = &ctx_dsi->panel_resource->params.dsi_params;
	if (params->mode_count == 0) {
		DDPPR_ERR("%s, invalid mode:%u\n",
			__func__, params->mode_count);
		return -EINVAL;
	}
	list_for_each_entry(mode_node, &params->mode_list, list) {
		if (drm_mode_equal(&mode_node->mode, mode) == true) {
			found = true;
			break;
		}
	}

	if (found == false) {
		DDPPR_ERR("%s: invalid dst id:%u\n", __func__, id);
		return -EINVAL;
	}

	*ext_param = &mode_node->ext_param;
	return 0;
}

static int mtk_panel_reset(struct drm_panel *panel, int on)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s, on:%d\n", __func__, on);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.reset != NULL)
		return cust->ext_funcs.reset(panel, on);

	ret = mtk_drm_gateic_reset(on, MTK_LCM_FUNC_DSI);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "%s:failed to reset panel %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}

static int mtk_panel_ata_check(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	u8 *data = NULL;
	int ret = 0, i = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.ata_check != NULL)
		return cust->ext_funcs.ata_check(panel);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, invalid ops\n", __func__);
		return 0;
	}

	if (ops->ata_id_value_length == 0 ||
		ops->ata_check.size == 0) {
		DDPMSG("%s, %d: not support ata id check, len:%u, size:%u\n",
			__func__, __LINE__, ops->ata_id_value_length,
			ops->ata_check.size);
		return 0;
	}

	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0) {
		DDPPR_ERR("%s, failed to create input packet\n", __func__);
		return 0;
	}


	if (mtk_lcm_create_input(input.data, ops->ata_id_value_length,
			MTK_LCM_INPUT_TYPE_READBACK) < 0) {
		DDPPR_ERR("%s, failed to create read buffer\n", __func__);
		goto fail;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->ata_check, ctx_dsi->panel_resource,
			&input, NULL, NULL, prop, "ata_check");
	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do ata check, %d\n",
			__func__, __LINE__, ret);
		ret = 0;
		goto end;
	}

	data = (u8 *)input.data->data;
	for (i = 0; i < ops->ata_id_value_length; i++) {
		DDPMSG("%s, i:%u expect:0x%x, get:0x%x\n", __func__, i,
			ops->ata_id_value_data[i], (unsigned int)data[i]);
		if (data[i] != ops->ata_id_value_data[i]) {
			ret = 0;
			goto end;
		}
	}
	ret = 1; /*1 for pass*/

end:
	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s-, %d\n", __func__, ret);
	return ret;
}

static int mtk_panel_aod_mode_check(unsigned char *out)
{
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	u8 *data = NULL;
	int ret = 0, i = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
		out == NULL) {
		DDPPR_ERR("%s, invalid ctx or out\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, invalid ops\n", __func__);
		return -EFAULT;
	}

	if (ops->aod_mode_value_length == 0 ||
		ops->aod_mode_check.size == 0) {
		DDPMSG("%s, %d: not support aod mode check, len:%u, size:%u\n",
			__func__, __LINE__, ops->aod_mode_value_length,
			ops->aod_mode_check.size);
		return 0;
	}

	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0) {
		DDPPR_ERR("%s, failed to create input packet\n", __func__);
		return -ENOMEM;
	}

	if (mtk_lcm_create_input(input.data, ops->aod_mode_value_length,
			MTK_LCM_INPUT_TYPE_READBACK) < 0) {
		DDPPR_ERR("%s, failed to create read buffer\n", __func__);
		ret = -ENOMEM;
		goto fail;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->aod_mode_check, ctx_dsi->panel_resource,
			&input, NULL, NULL, prop, "aod_mode_check");
	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do aod_mode check, %d\n",
			__func__, __LINE__, ret);
		goto end;
	}

	data = (u8 *)input.data->data;
	for (i = 0; i < ops->aod_mode_value_length; i++) {
		DDPMSG("%s, i:%u expect:0x%x, get:0x%x\n", __func__, i,
			ops->ata_id_value_data[i], (unsigned int)data[i]);
		if (data[i] != ops->ata_id_value_data[i]) {
			*out = 0;
			goto end;
		}
	}
	*out = 1;

end:
	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s-, ret:%d, out:0x%x\n", __func__, ret, *out);
	return ret;
}

static int panel_init_backlight_input_data(unsigned int level, unsigned int mode,
	struct mtk_lcm_ops_input *input)
{
	u8 *data = NULL;
	unsigned int count = 0;
	int ret = 0;

	if (mode <= 0xff) {
		count = 1;
		if (mtk_lcm_create_input(input, count,
				MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0) {
			DDPPR_ERR("%s, %d failed to alloc data\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto end;
		}
		data = (u8 *)input->data;
		*data = level;
	} else if (mode <= 0xffff) {
		count = 2;
		level = level * 4095 / 255;
		if (mtk_lcm_create_input(input, count,
				MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0) {
			DDPPR_ERR("%s, %d ended to alloc data\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto end;
		}
		data = (u8 *)input->data;
		data[0] = ((level >> 8) & 0xf);
		data[1] = (level & 0xff);
	} else {
		DDPPR_ERR("%s, %d, invalid backlight mode:0x%x\n",
			__func__, __LINE__, mode);
		ret = -EINVAL;
		goto end;
	}

	DDPINFO("%s, %d, mode:0x%x, level:0x%x, count:%u, data:0x%x, 0x%x\n",
		__func__, __LINE__, mode, level, count,
		data[0], count > 1 ? data[1] : 0);
end:
	return ret;
}

static int panel_set_backlight(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level, struct mtk_lcm_ops_table *table,
	unsigned int mode)
{
	struct mipi_dsi_device *dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_input_packet input;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	unsigned char aod_mode = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(table) || table->size == 0)
		return 0;

	if (level > 255)
		level = 255;

	if (mtk_lcm_create_input_packet(&input, 1, 2) < 0)
		return -ENOMEM;

	/* set runtime input data of backlight */
	if (panel_init_backlight_input_data(level, mode, &input.data[0]) < 0)
		goto fail2;

	/* set runtime input data of aod mode */
	if (mtk_lcm_create_input(&input.condition[0], 1,
			MTK_LCM_INPUT_TYPE_AOD) < 0) {
		DDPPR_ERR("%s, %d failed to alloc aod mode data\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto fail1;
	}
	if (atomic_read(&ctx_dsi->doze_had) != 0) {
		if (mtk_panel_aod_mode_check(&aod_mode) < 0)
			goto end;
		*(u8 *)input.condition[0].data = aod_mode;
	} else {
		*(u8 *)input.condition[0].data = 0;
	}

	/* set runtime input data of dim status */
	if (mtk_lcm_create_input(&input.condition[1], 1,
			MTK_LCM_INPUT_TYPE_DIM) < 0) {
		DDPPR_ERR("%s, %d failed to alloc dim data\n",
			__func__, __LINE__);
		ret = -ENOMEM;
		goto end;
	}
	if (atomic_read(&ctx_dsi->dim_status) == 0 &&
		level > 0) { //do dim on ops
		DDPMSG("%s, %d,switch dim on, level\n",
			__func__, __LINE__, level);
		*(u8 *)input.condition[1].data = 1;
	} else if (atomic_read(&ctx_dsi->dim_status) > 0 &&
		level == 0) { //do dim off ops
		DDPMSG("%s, %d,switch dim off, level\n",
			__func__, __LINE__, level);
		*(u8 *)input.condition[1].data = 0;
	} else { //skip dim ops
		*(u8 *)input.condition[1].data = 0xff;
	}

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev, table,
				ctx_dsi->panel_resource, &input, handle, NULL,
				prop, "set_backlight");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				table, &input, "set_backlight");
	if (ret < 0) {
		DDPPR_ERR("%s, %d failed to set backlight, %d\n", __func__, __LINE__, ret);
		goto end;
	}

	/* update context before return */
	atomic_set(&ctx_dsi->current_backlight, level);
	atomic_set(&ctx_dsi->doze_had, 0);
	if (atomic_read(&ctx_dsi->dim_status) == 0 &&
		level > 0)
		atomic_set(&ctx_dsi->dim_status, 1);
	else if (atomic_read(&ctx_dsi->dim_status) > 0 &&
		level == 0)
		atomic_set(&ctx_dsi->dim_status, 0);

end:
	mtk_lcm_destroy_input(input.condition);
fail1:
	mtk_lcm_destroy_input(input.data);
fail2:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s- level:%u %d\n", __func__, level, ret);
	return ret;
}

static int panel_set_backlight_group(void *dsi, dcs_grp_write_gce cb,
	void *handle, unsigned int level, struct mtk_lcm_ops_table *table,
	unsigned int mode)
{
	struct mipi_dsi_device *dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_input_packet input;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	unsigned int count = 0;
	u8 *data = NULL;
	int ret = 0;

	if (IS_ERR_OR_NULL(table) || table->size == 0)
		return 0;

	if (level > 255)
		level = 255;

	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0)
		return -ENOMEM;

	if (mode <= 0xff) {
		count = 1;
		if (mtk_lcm_create_input(input.data, count,
				MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0) {
			DDPPR_ERR("%s, %d failed to alloc data\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto fail;
		}
		data = (u8 *)input.data->data;
		*data = level;
	} else if (mode <= 0xffff) {
		count = 2;
		level = level * 4095 / 255;
		if (mtk_lcm_create_input(input.data, count,
				MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0) {
			DDPPR_ERR("%s, %d failed to alloc data\n", __func__, __LINE__);
			ret = -ENOMEM;
			goto fail;
		}
		data = (u8 *)input.data->data;
		data[0] = ((level >> 8) & 0xf);
		data[1] = (level & 0xff);
	} else {
		DDPPR_ERR("%s, %d, invalid backlight mode:0x%x\n",
			__func__, __LINE__, mode);
		ret = -EINVAL;
		goto fail;
	}

	DDPINFO("%s, %d, mode:0x%x, level:0x%x, count:%u, data:0x%x, 0x%x\n",
		__func__, __LINE__, mode, level, count,
		data[0], count > 1 ? data[1] : 0);

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev, table,
				ctx_dsi->panel_resource, &input, handle, NULL,
				prop, "set_backlight_grp");
	else
		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
				table, &input, "set_backlight_grp");
	atomic_set(&ctx_dsi->current_backlight, level);

	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	return ret;
}

static int mtk_panel_set_backlight_cmdq(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level)
{
	//struct mtk_panel_context *ctx_dsi = mipi_dsi_get_drvdata(dsi);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	unsigned int mode = 0;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.set_backlight_cmdq != NULL) {
		ret = cust->ext_funcs.set_backlight_cmdq(dsi, cb, handle, level);
		if (ret < 0) {
			DDPPR_ERR("%s, %d failed to set backlight, %d\n", __func__, __LINE__, ret);
			return ret;
		}
		atomic_set(&ctx_dsi->current_backlight, level);
		return ret;
	}

	if (atomic_read(&ctx_dsi->doze_enabled) != 0) {
		DDPMSG("%s,%d: skip set backlight when aod on\n",
			__func__, __LINE__);
		return 0;
	}
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
		ops->set_backlight_cmdq.size == 0) {
		DDPPR_ERR("%s, invalid backlight table\n", __func__);
		return -EINVAL;
	}

	table = &ops->set_backlight_cmdq;
	mode = ops->set_backlight_mask;

	ret = panel_set_backlight(dsi, cb, handle,
				level, table, mode);
	return ret;
}

static int mtk_panel_set_backlight_grp_cmdq(void *dsi, dcs_grp_write_gce cb,
	void *handle, unsigned int level)
{
	//struct mtk_panel_context *ctx_dsi = mipi_dsi_get_drvdata(dsi);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	unsigned int mode = 0;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.set_backlight_grp_cmdq != NULL) {
		ret = cust->ext_funcs.set_backlight_grp_cmdq(dsi, cb, handle, level);
		if (ret < 0) {
			DDPPR_ERR("%s, %d failed to set backlight, %d\n", __func__, __LINE__, ret);
			return ret;
		}
		atomic_set(&ctx_dsi->current_backlight, level);
		return ret;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
		ops->set_backlight_cmdq.size == 0) {
		DDPPR_ERR("%s, invalid backlight table\n", __func__);
		return -EINVAL;
	}

	table = &ops->set_backlight_cmdq;
	mode = ops->set_backlight_mask;

	ret = panel_set_backlight_group(dsi, cb, handle,
				level, table, mode);
	return ret;
}

static int mtk_panel_get_virtual_heigh(void)
{
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource))
		return 0;

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.get_virtual_heigh != NULL)
		return cust->ext_funcs.get_virtual_heigh();

	return ctx_dsi->panel_resource->params.resolution[1];
}

static int mtk_panel_get_virtual_width(void)
{
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource))
		return 0;

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.get_virtual_width != NULL)
		return cust->ext_funcs.get_virtual_width();

	return ctx_dsi->panel_resource->params.resolution[0];
}

static int mtk_panel_mode_switch(struct drm_panel *panel,
		struct drm_connector *connector,
		unsigned int cur_mode, unsigned int dst_mode,
		enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_table *table = NULL;
	struct mtk_lcm_params *params = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct drm_display_mode *mode = NULL;
	char owner[MAX_PANEL_OPERATION_NAME] = {0};
	struct mtk_lcm_mode_dsi *mode_node;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	bool found = false, default_switch = false;
	const struct mtk_panel_cust *cust = NULL;
	struct mtk_lcm_ops_input_packet input;
	unsigned long flags = 0;
	u8 *fps_switch_mode = NULL;
	unsigned int mode_count = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (cur_mode == dst_mode)
		return 0;

	DDPMSG("%s+, cur:%u, dst:%u, stage:%d, powerdown:%d\n",
		__func__, cur_mode, dst_mode, stage, BEFORE_DSI_POWERDOWN);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.mode_switch != NULL)
		return cust->ext_funcs.mode_switch(panel,
				connector, cur_mode, dst_mode, stage);

	params = &ctx_dsi->panel_resource->params;
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	mode = get_mode_by_connector_id(connector, dst_mode);
	if (IS_ERR_OR_NULL(params) ||
		params->dsi_params.mode_count == 0 ||
		IS_ERR_OR_NULL(ops) ||
		IS_ERR_OR_NULL(mode))
		return -EINVAL;

	list_for_each_entry(mode_node, &params->dsi_params.mode_list, list) {
		if (drm_mode_equal(&mode_node->mode, mode) == true) {
			found = true;
			break;
		}
	}

	if (found == false) {
		DDPPR_ERR("%s: invalid dst mode:%u\n", __func__, dst_mode);
		return -EINVAL;
	}

	switch (stage) {
	case BEFORE_DSI_POWERDOWN:
		if (mode_node->fps_switch_bfoff.size > 0) {
			table = &mode_node->fps_switch_bfoff;
			ret = snprintf(owner, sizeof(owner),
					"private_fps_switch_bfoff_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
		} else if (ops->default_fps_switch_bfoff.size > 0) {
			table = &ops->default_fps_switch_bfoff;
			default_switch = true;
			fps_switch_mode = &ops->fps_switch_bfoff_mode[0];
			mode_count = ops->fps_switch_bfoff_mode == NULL ?
					0 : ops->fps_switch_bfoff_mode_count;
			ret = snprintf(owner, sizeof(owner),
					"default_fps_switch_bfoff_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
		} else {
			DDPINFO("%s, %d, invalid fps_switch_bfoff table\n",
				__func__, __LINE__);
			return 0;
		}
		if (ret < 0 || (size_t)ret >= sizeof(owner))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
		break;
	case AFTER_DSI_POWERON:
		if (mode_node->fps_switch_afon.size > 0) {
			table = &mode_node->fps_switch_afon;
			ret = snprintf(owner, sizeof(owner),
					"private_fps_switch_afon_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
		} else if (ops->default_fps_switch_afon.size > 0) {
			table = &ops->default_fps_switch_afon;
			default_switch = true;
			fps_switch_mode = &ops->fps_switch_afon_mode[0];
			mode_count = ops->fps_switch_afon_mode == NULL ?
					0 : ops->fps_switch_afon_mode_count;
			ret = snprintf(owner, sizeof(owner),
					"default_fps_switch_afon_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
		} else {
			DDPINFO("%s, %d, invalid fps_switch_afon table\n",
				__func__, __LINE__);
			return 0;
		}
		if (ret < 0 || (size_t)ret >= sizeof(owner))
			DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
		break;
	default:
		DDPPR_ERR("%s: invalid stage:%d\n", __func__, stage);
		return -EINVAL;
	}

	if (default_switch == true) {
		int i = 0;

		if (mode_count == 0) {
			DDPPR_ERR("%s,%d, doesn't support default_fps_switch\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		if (mtk_lcm_create_input_packet(&input, 0, 1) < 0)
			return -ENOMEM;

		/* set mode index */
		if (mtk_lcm_create_input(&input.condition[0], 1,
				MTK_LCM_INPUT_TYPE_CURRENT_MODE) < 0) {
			DDPPR_ERR("%s, %d: failed to alloc mode\n",
				__func__, __LINE__);
			ret = -ENOMEM;
			goto fail;
		}
		spin_lock_irqsave(&ctx_dsi->lock, flags);
		*(u8 *)input.condition[0].data = ctx_dsi->current_mode->id;
		for (i = 0; i < mode_count; i++) {
			if (fps_switch_mode[i] == ctx_dsi->current_mode->id)
				break;
		}
		spin_unlock_irqrestore(&ctx_dsi->lock, flags);

		/* some mode doesn't support fps_switch*/
		if (i == mode_count) {
			ret = 0;
			DDPPR_ERR("%s,%d, mode:%u doesn't support default_fps_switch\n",
				__func__, __LINE__, ctx_dsi->current_mode->id);
			goto fail2;
		}
	}

	ret = mtk_panel_execute_operation(dsi_dev, table,
				ctx_dsi->panel_resource,
				&input, NULL, NULL, prop, owner);

fail2:
	if (default_switch == true)
		mtk_lcm_destroy_input(input.condition);
fail:
	if (default_switch == true)
		mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s-, %s,ret:%d\n", __func__, owner, ret);
	return ret;
}

static int mtk_panel_set_aod_light_mode(void *dsi,
	dcs_write_gce cb, void *handle, unsigned int level)
{
	//struct mtk_panel_context *ctx_dsi = mipi_dsi_get_drvdata(dsi);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	unsigned int light_mask = 0;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.set_aod_light_mode != NULL)
		return cust->ext_funcs.set_aod_light_mode(dsi,
				cb, handle, level);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
		ops->set_aod_light.size == 0) {
		DDPPR_ERR("%s, invalid aod light mod table\n", __func__);
		return -EINVAL;
	}

	light_mask = ops->set_aod_light_mask;
	table = &ops->set_aod_light;

	ret = panel_set_backlight(dsi, cb, handle,
				level, table, light_mask);

	return ret;
}

static int mtk_panel_doze_enable_start(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.doze_enable_start != NULL)
		return cust->ext_funcs.doze_enable_start(panel,
				dsi, cb, handle);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops) ||
		ops->doze_enable_start.size == 0)
		return -EINVAL;

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->doze_enable_start, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "doze_enable_start");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->doze_enable_start, NULL, "doze_enable_start");

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.doze_enable != NULL)
		return cust->ext_funcs.doze_enable(panel,
				dsi, cb, handle);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops) ||
		ops->doze_enable.size == 0)
		return -EINVAL;

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->doze_enable, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "doze_enable");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->doze_enable, NULL, "doze_enable");

	if (ret == 0) {
		atomic_set(&ctx_dsi->doze_enabled, 1);
		atomic_set(&ctx_dsi->doze_had, 1);
	}

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.doze_disable != NULL)
		return cust->ext_funcs.doze_disable(panel,
				dsi, cb, handle);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops) ||
		ops->doze_disable.size == 0)
		return -EINVAL;

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->doze_disable, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "doze_disable");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->doze_disable, NULL, "doze_disable");

	if (ret == 0)
		atomic_set(&ctx_dsi->doze_enabled, 0);
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_post_disp_on(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.doze_post_disp_on != NULL)
		return cust->ext_funcs.doze_post_disp_on(panel,
				dsi, cb, handle);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops) ||
		ops->doze_post_disp_on.size == 0)
		return -EINVAL;

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->doze_post_disp_on, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "doze_post_disp_on");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->doze_post_disp_on, NULL, "doze_post_disp_on");

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.doze_area != NULL)
		return cust->ext_funcs.doze_area(panel,
				dsi, cb, handle);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops) ||
		ops->doze_area.size == 0)
		return -EINVAL;

	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->doze_area, ctx_dsi->panel_resource,
				NULL, handle, NULL, prop, "doze_area");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->doze_area, NULL, "doze_area");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static unsigned long mtk_panel_doze_get_mode_flags(
	struct drm_panel *panel, int doze_en)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_params *params = NULL;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	DDPMSG("%s+\n", __func__);

	if (cust != NULL &&
		cust->ext_funcs.doze_get_mode_flags != NULL)
		return cust->ext_funcs.doze_get_mode_flags(panel,
				doze_en);

	params = &ctx_dsi->panel_resource->params;
	if (IS_ERR_OR_NULL(params))
		return 0;

	if (doze_en == 0)
		return params->dsi_params.mode_flags_doze_off;

	return params->dsi_params.mode_flags_doze_on;
}

static int mtk_panel_hbm_set_cmdq(struct drm_panel *panel, void *dsi,
				  dcs_write_gce cb, void *handle, bool en)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input = {0};
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	if (atomic_read(&ctx_dsi->hbm_en) == en)
		return 0;

	DDPMSG("%s+\n", __func__);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.hbm_set_cmdq != NULL) {
		ret = cust->ext_funcs.hbm_set_cmdq(panel,
				dsi, cb, handle, en);
		if (ret < 0) {
			DDPPR_ERR("%s, %d, failed to execute hbm set cmdq\n",
				__func__, __LINE__);
			return ret;
		}

		goto end;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
		ops->hbm_set_cmdq.size == 0)
		return -EINVAL;

	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0)
		return -EFAULT;

	if (mtk_lcm_create_input(input.data, 1,
			MTK_LCM_INPUT_TYPE_MISC) < 0)
		goto fail;

	if (en)
		*(u8 *)input.data->data =
				ops->hbm_set_cmdq_switch_on;
	else
		*(u8 *)input.data->data =
				ops->hbm_set_cmdq_switch_off;
	if (mtk_lcm_support_cb == 0)
		ret = mtk_panel_execute_operation(dsi_dev,
				&ops->hbm_set_cmdq, ctx_dsi->panel_resource,
				&input, handle, NULL, prop, "hbm_set_cmdq");
	else
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				&ops->hbm_set_cmdq, &input, "hbm_set_cmdq");
	if (ret < 0) {
		DDPPR_ERR("%s, %d, failed to execute hbm set cmdq\n",
			__func__, __LINE__);
		goto end;
	}

end:
	atomic_set(&ctx_dsi->hbm_en, en);
	atomic_set(&ctx_dsi->hbm_wait, 1);
	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static void mtk_panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	const struct mtk_panel_cust *cust = ctx_dsi->panel_resource->cust;

	if (cust != NULL &&
		cust->ext_funcs.hbm_get_state != NULL)
		return cust->ext_funcs.hbm_get_state(panel, state);

	*state = atomic_read(&ctx_dsi->hbm_en);
}

static void mtk_panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	const struct mtk_panel_cust *cust = ctx_dsi->panel_resource->cust;

	if (cust != NULL &&
		cust->ext_funcs.hbm_get_wait_state != NULL)
		return cust->ext_funcs.hbm_get_wait_state(panel, wait);

	*wait = atomic_read(&ctx_dsi->hbm_wait);
}

static bool mtk_panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	bool old = atomic_read(&ctx_dsi->hbm_wait);
	const struct mtk_panel_cust *cust = ctx_dsi->panel_resource->cust;

	if (cust != NULL &&
		cust->ext_funcs.hbm_set_wait_state != NULL)
		return cust->ext_funcs.hbm_set_wait_state(panel, wait);

	atomic_set(&ctx_dsi->hbm_wait, wait);
	return old;
}

static void mtk_panel_dump(struct drm_panel *panel, enum MTK_LCM_DUMP_FLAG flag)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_panel_resource *resource = ctx_dsi->panel_resource;

	DDPMSG("%s, %d, flag:%d\n", __func__, __LINE__, flag);
	switch (flag) {
	case MTK_DRM_PANEL_DUMP_PARAMS:
		dump_lcm_params_basic(&resource->params);
		dump_lcm_params_dsi(&resource->params.dsi_params, resource->cust);
		break;
	case MTK_DRM_PANEL_DUMP_OPS:
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, resource->cust);
		break;
	case MTK_DRM_PANEL_DUMP_ALL:
		dump_lcm_params_basic(&resource->params);
		dump_lcm_params_dsi(&resource->params.dsi_params, resource->cust);
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, resource->cust);
		break;
	default:
		break;
	}
}

static struct mtk_lcm_mode_dsi *mtk_drm_panel_get_mode_by_id(
	struct mtk_lcm_params_dsi *params, unsigned int id)
{
	struct mtk_lcm_mode_dsi *mode_node;

	if (IS_ERR_OR_NULL(params) ||
		params->mode_count == 0)
		return NULL;

	list_for_each_entry(mode_node, &params->mode_list, list) {
		if (mode_node->id != id)
			continue;

		return mode_node;
	}

	return NULL;
}

static struct mtk_lcm_mode_dsi *mtk_lcm_find_1st_max_fps_mode(unsigned int level)
{
	struct mtk_lcm_mode_dsi *mode_node;
	struct mtk_lcm_mode_dsi *max_node = {0};
	unsigned int max_fps = (unsigned int)-1;
	struct mtk_lcm_params_dsi *params =
			&ctx_dsi->panel_resource->params.dsi_params;

	list_for_each_entry(mode_node, &params->mode_list, list) {
		if (mode_node->fps >= level &&
			mode_node->fps < max_fps) {
			max_fps = mode_node->fps;
			max_node = mode_node;
		}
	}
	DDPMSG("%s, %d, level:%u, max_fps:%d\n",
		__func__, __LINE__, level, (int)max_fps);

	return max_node;
}

static unsigned int mtk_lcm_find_1st_min_fps_switch(
	unsigned int level, struct mtk_lcm_mode_dsi *mode_node)
{
	unsigned int min_fps = 0;
	unsigned int i = 0;

	for (i = 0; i < mode_node->msync_set_min_fps_list_length; i++) {
		if (mode_node->msync_set_min_fps_list[i] <= level &&
			mode_node->msync_set_min_fps_list[i] > min_fps) {
			min_fps = mode_node->msync_set_min_fps_list[i];
		}
	}
	DDPMSG("%s, %d, level:%u, min_fps:%u\n",
		__func__, __LINE__, level, min_fps);

	return min_fps;
}

#define MTK_LCM_MTE_REQUEST (0xEEEE)
#define MTK_LCM_MTE_OFF (0xFFFF)
static int mtk_panel_msync_te_level_switch(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int fps_level)
{
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	const struct mtk_panel_cust *cust = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	char owner[MAX_PANEL_OPERATION_NAME] = {0};
	struct mtk_lcm_ops_input_packet input;
	//struct mtk_panel_ext *ext = find_panel_ext(panel);
	bool default_switch = false;
	unsigned long flags = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s:%d fps_level:0x%x\n", __func__, __LINE__, fps_level);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.msync_te_level_switch != NULL)
		return cust->ext_funcs.msync_te_level_switch(dsi, cb,
				handle, fps_level);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, %d, invalid msync operation\n", __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (fps_level == MTK_LCM_MTE_REQUEST) { /*request multi te */
		DDPMSG("%s:%d request MTE\n", __func__, __LINE__);
		if (ops->msync_request_mte.size == 0) {
			DDPPR_ERR("%s, %d, invalid msync request operation\n", __func__, __LINE__);
			ret = -EINVAL;
			goto out;
		}

		ret = mtk_panel_execute_callback(dsi, cb, handle,
			&ops->msync_request_mte, NULL, "msync_request_mte");
	} else if (fps_level == MTK_LCM_MTE_OFF) { /*close multi te */
		DDPMSG("%s:%d Close MTE\n", __func__, __LINE__);
		if (ops->default_msync_close_mte.size == 0) {
			DDPPR_ERR("%s, %d, invalid msync close operation\n",
				__func__, __LINE__);
			ret = -EINVAL;
			goto out;
		}

		ret = mtk_panel_execute_callback(dsi, cb, handle,
			&ops->default_msync_close_mte, NULL, "default_msync_close_mte");
		ret = mtk_panel_execute_callback(dsi, cb, handle,
			&ops->msync_default_mte, NULL, "msync_default_mte");
	} else {
		mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
		if (mode_node != NULL) {
			if (mode_node->msync_switch_mte.size > 0) {
				table = &mode_node->msync_switch_mte;
				ret = snprintf(owner, sizeof(owner),
					"private_msync_switch_mte_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(owner))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			} else if (ops->default_msync_switch_mte.size > 0) {
				default_switch = true;
				table = &ops->default_msync_switch_mte;
				ret = snprintf(owner, sizeof(owner),
					"default_msync_switch_mte_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(owner))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			} else {
				DDPPR_ERR("%s,%d: invalid msync_switch_mte table\n",
					__func__, __LINE__);
				ret = -EFAULT;
				goto out;
			}

			if (default_switch == true) {
				int i = 0, count = 0;

				count = ops->msync_switch_mte_mode == NULL ?
						0 : ops->msync_switch_mte_mode_count;
				if (count == 0) {
					DDPPR_ERR("%s, %d, not support msync switch mte\n",
						__func__, __LINE__);
					return -EFAULT;
				}

				if (mtk_lcm_create_input_packet(&input, 0, 1) < 0)
					return -ENOMEM;

				if (mtk_lcm_create_input(input.condition, 1,
						MTK_LCM_INPUT_TYPE_CURRENT_MODE) < 0) {
					DDPPR_ERR("%s, %d: failed to alloc condition data\n",
						__func__, __LINE__);
					ret = -ENOMEM;
					goto fail;
				}
				spin_lock_irqsave(&ctx_dsi->lock, flags);
				*(unsigned int *)input.condition->data = mode_node->id;
				for (i = 0; i < count; i++) {
					if (ops->msync_switch_mte_mode[i] == mode_node->id)
						break;
				}
				spin_unlock_irqrestore(&ctx_dsi->lock, flags);

				/* some mode doesn't support msync_switch_mte*/
				if (i == count) {
					ret = 0;
					DDPPR_ERR("%s,%d, mode:%u invalid table\n",
						__func__, __LINE__, mode_node->id);
					goto fail2;
				}
			} else {
				input.data_count = 0;
				input.condition_count = 0;
				input.data = NULL;
				input.condition = NULL;
			}

			DDPMSG("%s:%d switch to fps:%u, level:0x%x, owner:%s\n",
				__func__, __LINE__, mode_node->fps,
				fps_level, owner);

			ret = mtk_panel_execute_callback(dsi, cb, handle,
				table, &input, owner);

			DDPMSG("%s, %d, ret:%d\n", __func__, __LINE__, ret);
		} else {
			DDPMSG("%s, %d, failed to find fps node of level:0x%x\n",
				__func__, __LINE__, fps_level);
			ret = 1;
			goto out;
		}
	}

	DDPMSG("%s, %d, ret:%d\n", __func__, __LINE__, ret);

fail2:
	if (default_switch == true)
		mtk_lcm_destroy_input(input.condition);
fail:
	if (default_switch == true)
		mtk_lcm_destroy_input_packet(&input);
out:
	DDPMSG("%s:%d fps_level:0x%x, ret:%u\n",
		__func__, __LINE__, fps_level, ret);
	return ret;
}

static int mtk_panel_msync_te_level_switch_grp(void *dsi, dcs_grp_write_gce cb,
		void *handle, struct drm_panel *panel, unsigned int fps_level)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	struct mtk_lcm_ops_dsi *ops = NULL;
	char owner[MAX_PANEL_OPERATION_NAME] = {0};
	struct mtk_lcm_ops_input_packet input;
	bool default_switch = false;
	unsigned long flags = 0;
	const struct mtk_panel_cust *cust = NULL;
	struct mtk_panel_ext *ext = find_panel_ext(panel);
	int ret = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.msync_te_level_switch_grp != NULL)
		return cust->ext_funcs.msync_te_level_switch_grp(dsi, cb,
				handle, panel, fps_level);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, %d, invalid msync operation\n", __func__, __LINE__);
		ret = -EINVAL;
		goto out;
	}

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (fps_level == MTK_LCM_MTE_REQUEST) { /*request multi te */
		DDPMSG("%s:%d request MTE ++\n", __func__, __LINE__);
		if (ops->msync_request_mte.size == 0) {
			DDPMSG("%s, %d, not support msync request,0x%x\n",
				__func__, __LINE__, fps_level);
			ret = 1;
			goto out;
		}

		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
			&ops->msync_request_mte, NULL, "msync_request_mte");
	} else if (fps_level == MTK_LCM_MTE_OFF) { /*close multi te */
		DDPMSG("%s:%d Close MTE\n", __func__, __LINE__);
		if (ops->default_msync_close_mte.size == 0) {
			DDPPR_ERR("%s, %d, not support msync close,0x%x\n",
				__func__, __LINE__, fps_level);
			ret = -EINVAL;
			goto out;
		}

		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
			&ops->default_msync_close_mte, NULL, "default_msync_close_mte");
		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
			&ops->msync_default_mte, NULL, "msync_default_mte");
		if (ret == 0 && ext != NULL) {
			ext->params->pll_clk = ctx_dsi->current_mode->ext_param.pll_clk;
			ext->params->data_rate = ctx_dsi->current_mode->ext_param.data_rate;
		}
	} else {
		mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
		if (mode_node != NULL) {
			if (mode_node->msync_switch_mte.size > 0) {
				table = &mode_node->msync_switch_mte;
				ret = snprintf(owner, sizeof(owner),
					"private_msync_switch_mte_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(owner))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			} else if (ops->default_msync_switch_mte.size > 0) {
				default_switch = true;
				table = &ops->default_msync_switch_mte;
				ret = snprintf(owner, sizeof(owner),
					"default_msync_switch_mte_%u-%u-%u",
					mode_node->width, mode_node->height,
					mode_node->fps);
				if (ret < 0 || (size_t)ret >= sizeof(owner))
					DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
			} else {
				DDPPR_ERR("%s,%d: not support msync_switch_mte,0x%x\n",
					__func__, __LINE__, fps_level);
				ret = -EFAULT;
				goto out;
			}

			if (default_switch == true) {
				int i = 0, count = 0;

				count = ops->msync_switch_mte_mode == NULL ?
						0 : ops->msync_switch_mte_mode_count;
				if (count == 0) {
					DDPPR_ERR("%s, %d, not support msync switch mte\n",
						__func__, __LINE__);
					return -EFAULT;
				}

				if (mtk_lcm_create_input_packet(&input, 0, 1) < 0)
					return -ENOMEM;

				if (mtk_lcm_create_input(input.condition, 1,
						MTK_LCM_INPUT_TYPE_CURRENT_MODE) < 0) {
					DDPPR_ERR("%s, %d: failed to alloc condition data\n",
						__func__, __LINE__);
					ret = -ENOMEM;
					goto fail;
				}
				spin_lock_irqsave(&ctx_dsi->lock, flags);
				*(unsigned int *)input.condition->data = mode_node->id;
				for (i = 0; i < count; i++) {
					if (ops->msync_switch_mte_mode[i] == mode_node->id)
						break;
				}
				spin_unlock_irqrestore(&ctx_dsi->lock, flags);

				/* some mode doesn't support msync_switch_mte*/
				if (i == count) {
					ret = 0;
					DDPPR_ERR("%s,%d, mode:%u invalid table\n",
						__func__, __LINE__, mode_node->id);
					goto fail2;
				}
			} else {
				input.data_count = 0;
				input.condition_count = 0;
				input.data = NULL;
				input.condition = NULL;
			}

			DDPMSG("%s:%d switch to fps:%u, level:0x%x, owner:%s\n",
				__func__, __LINE__, mode_node->fps,
				fps_level, owner);

			ret = mtk_panel_execute_callback_group(dsi, cb,
				handle, table, &input, owner);

			if (ret == 0 && ext != NULL) {
				ext->params->pll_clk = mode_node->ext_param.pll_clk;
				ext->params->data_rate = mode_node->ext_param.data_rate;
			}
		} else {
			DDPMSG("%s, %d, failed to find fps node of level:0x%x\n",
				__func__, __LINE__, fps_level);
			ret = 1;
			goto out;
		}
	}

fail2:
	if (default_switch == true)
		mtk_lcm_destroy_input(input.condition);
fail:
	if (default_switch == true)
		mtk_lcm_destroy_input_packet(&input);
out:
	if (ext != NULL)
		DDPMSG("%s:%d fps_level:0x%x,ret:%u,mode:%d,pll:%u,data_rate:%u\n",
			__func__, __LINE__, fps_level, ret,
			ctx_dsi->current_mode->id,
			ext->params->pll_clk, ext->params->data_rate);
	else
		DDPMSG("%s:%d fps_level:0x%x,ret:%u,mode:%d\n",
			__func__, __LINE__, fps_level, ret,
			ctx_dsi->current_mode->id);

	return ret;
}

static int mtk_panel_msync_cmd_set_min_fps(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int flag)
{
	unsigned int fps_level = (flag & 0xFFFF0000) >> 16;
	unsigned int min_fps = flag & 0xFFFF;
	struct mtk_lcm_mode_dsi *mode_node;
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input;
	int ret = 0;
	const struct mtk_panel_cust *cust = NULL;
	unsigned int fps = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	/* When MTE off, min fps need set to vrefresh*/
	if (fps_level == MTK_LCM_MTE_OFF)
		fps_level = min_fps;

	DDPMSG("%s:%d flag:0x%08x, fps_level:0x%x min_fps:%u\n",
			__func__, __LINE__, flag, fps_level, min_fps);
	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.msync_cmd_set_min_fps != NULL)
		return cust->ext_funcs.msync_cmd_set_min_fps(dsi, cb,
				handle, flag);

	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	ops = ctx_dsi->panel_resource->ops.dsi_ops;

	/* get target mode */
	mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
	if (mode_node == NULL) {
		DDPPR_ERR("%s, %d, failed to get max fps mode, level:0x%x\n",
			__func__, __LINE__, fps_level);
		return -EFAULT;
	}
	if (mode_node->msync_set_min_fps.size == 0) {
		DDPPR_ERR("%s, %d, invalid msync_set_min_fps table\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	/* get target min fps */
	fps = mtk_lcm_find_1st_min_fps_switch(min_fps, mode_node);
	if (fps == 0) {
		DDPPR_ERR("%s, %d, failed to get min fps switch, level:%u\n",
			__func__, __LINE__, min_fps);
		return -EFAULT;
	}

	/* init runtime input data */
	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0)
		return -EFAULT;

	if (mtk_lcm_create_input(input.data, 1,
			MTK_LCM_INPUT_TYPE_CURRENT_FPS) < 0)
		goto fail;

	*(u8 *)input.data->data = fps;
	ret = mtk_panel_execute_callback(dsi, cb, handle,
			&mode_node->msync_set_min_fps, &input, "msync_set_min_fps");

	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s:%d get min fps switch:%u, level:%u, ret:%d\n",
		__func__, __LINE__, fps, min_fps, ret);
	return ret;
}

static enum mtk_lcm_version mtk_panel_get_lcm_version(void)
{
	return MTK_COMMON_LCM_DRV;
}

static int mtk_panel_ddic_ops(struct drm_panel *panel, enum MTK_PANEL_DDIC_OPS ops,
	struct mtk_lcm_dsi_cmd_packet *packet, void *misc)
{
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.ddic_ops != NULL)
		return cust->ext_funcs.ddic_ops(panel, ops,
				packet, misc);

	switch (ops) {
	case MTK_PANEL_DESTROY_DDIC_PACKET:
		mtk_lcm_destroy_ddic_packet(packet);
		break;
	default:
		break;
	}

	return 0;
}

static int mtk_panel_cust_funcs(struct drm_panel *panel, int cmd, void *params,
	void *handle, void **output)
{
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust->cust_funcs != NULL)
		return cust->cust_funcs(panel, cmd, params, handle, output);

	return -EINVAL;
}

static int mtk_panel_read_panelid(struct drm_panel *panel, struct mtk_oddmr_panelid *panelid)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input;
	unsigned int prop = MTK_LCM_DSI_CMD_PROP_CMDQ |
				MTK_LCM_DSI_CMD_PROP_CMDQ_FORCE |
				MTK_LCM_DSI_CMD_PROP_PACK;
	u8 *data = NULL;
	const struct mtk_panel_cust *cust = NULL;
#ifdef LCM_DEBUG_PANLEID
	unsigned int i = 0;
#endif
	unsigned int len = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(panelid)) {
		DDPPR_ERR("%s, invalid output buffer\n", __func__);
		return -EINVAL;
	}

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.read_panelid != NULL)
		return cust->ext_funcs.read_panelid(panel, panelid);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	len = ops->read_panelid_len;
	if (IS_ERR_OR_NULL(ops) || len == 0) {
		DDPPR_ERR("%s, invalid ops, len:%u\n", __func__, len);
		return -EFAULT;
	}

	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0) {
		DDPPR_ERR("%s, failed to create input packet\n", __func__);
		return -ENOMEM;
	}

	if (mtk_lcm_create_input(input.data, len,
			MTK_LCM_INPUT_TYPE_READBACK) < 0) {
		DDPPR_ERR("%s, failed to create read buffer\n", __func__);
		goto fail;
	}

	ret = mtk_panel_execute_operation(dsi_dev,
			&ops->read_panelid, ctx_dsi->panel_resource,
			&input, NULL, NULL, prop, "read_panelid");
	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to read_panelid, %d\n",
			__func__, __LINE__, ret);
		goto end;
	}

	data = (u8 *)input.data->data;
	memcpy(panelid->data, data, len);
	panelid->len = len;
	ret = len;

#ifdef LCM_DEBUG_PANLEID
	for (i = 0; i < len; i++) {
		DDPMSG("%s, panelid[%u]= 0x%x 0x%x\n", __func__, i,
			(unsigned int)data[i], (unsigned int)panelid->data[i]);
	}
#endif

end:
	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s-, len:%u\n", __func__, panelid->len);
	return ret;
}

static int mtk_panel_set_bl_elvss_cmdq(void *dsi, dcs_grp_write_gce cb,
				 void *handle, struct mtk_bl_ext_config *bl_ext_config)
{
	struct mtk_lcm_ops_dsi *ops = NULL;
	struct mtk_lcm_ops_table *table = NULL;
	struct mipi_dsi_device *dsi_dev = NULL;
	struct mtk_lcm_ops_input_packet input;
	const struct mtk_panel_cust *cust = NULL;
	u8 *data = NULL;
	int pulses = 0, ret = 0;
	char owner[128] = { 0 };
	bool update_bl = false;

	if (cb == NULL || bl_ext_config == NULL) {
		DDPPR_ERR("%s, %d, invalid param\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->ext_funcs.set_bl_elvss_cmdq != NULL)
		return cust->ext_funcs.set_bl_elvss_cmdq(dsi,
				cb, handle, bl_ext_config);

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	dsi_dev = to_mipi_dsi_device(ctx_dsi->dev);
	if (IS_ERR_OR_NULL(ops)) {
		DDPPR_ERR("%s, invalid ops\n", __func__);
		return -EFAULT;
	}


	if (bl_ext_config->elvss_pn < 1)
		pulses = 1;
	else if (bl_ext_config->elvss_pn > 41)
		pulses = 41;
	else
		pulses = bl_ext_config->elvss_pn;

	if ((bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL)) &&
			(bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))) {
		table = &ops->set_backlight_elvss_cmdq;
		if (table == NULL) {
			DDPPR_ERR("%s, invalid bl_elvss table\n", __func__);
			return -EFAULT;
		}

		ret = snprintf(owner, sizeof(owner), "set_backlight_elvss");
		if (ret < 0) {
			DDPMSG("%s, %d, failed to set owner:bl_elvss, ret:%d\n",
				__func__, __LINE__, ret);
			ret = 0;
		}

		if (mtk_lcm_create_input_packet(&input, 2, 0) < 0) {
			DDPPR_ERR("%s, failed to create input packet\n", __func__);
			return -ENOMEM;
		}

		/*init backlight input data*/
		ret = panel_init_backlight_input_data(bl_ext_config->backlight_level,
					ops->set_backlight_mask, &input.data[0]);
		if (ret < 0)
			goto fail;
		update_bl = true;

		/*init elvss input data*/
		ret = mtk_lcm_create_input(&input.data[1], 1,
				MTK_LCM_INPUT_TYPE_MISC);
		if (ret < 0) {
			DDPPR_ERR("%s, %d failed to alloc elvss data\n", __func__, __LINE__);
			goto end;
		}
		data = (u8 *)input.data[1].data;
		*data = (u8)((1<<7)|pulses);
	} else if ((bl_ext_config->cfg_flag & (0x1<<SET_BACKLIGHT_LEVEL))) {
		table = &ops->set_backlight_cmdq;
		if (table == NULL) {
			DDPPR_ERR("%s, invalid bl table\n", __func__);
			return -EFAULT;
		}

		ret = snprintf(owner, sizeof(owner), "set_backlight");
		if (ret < 0) {
			DDPMSG("%s, %d, failed to set owner:bl, ret:%d\n",
				__func__, __LINE__, ret);
			ret = 0;
		}

		if (mtk_lcm_create_input_packet(&input, 1, 0) < 0) {
			DDPPR_ERR("%s, failed to create input packet\n", __func__);
			return -ENOMEM;
		}

		/*init backlight input data*/
		ret = panel_init_backlight_input_data(bl_ext_config->backlight_level,
					ops->set_backlight_mask, input.data);
		if (ret < 0)
			goto fail;
		update_bl = true;
	} else if ((bl_ext_config->cfg_flag & (0x1<<SET_ELVSS_PN))) {
		table = &ops->set_elvss_cmdq;
		if (table == NULL) {
			DDPPR_ERR("%s, invalid elvss table\n", __func__);
			return -EFAULT;
		}

		ret = snprintf(owner, sizeof(owner), "set_elvss");
		if (ret < 0) {
			DDPMSG("%s, %d, failed to set owner:elvss, ret:%d\n",
				__func__, __LINE__, ret);
			ret = 0;
		}

		if (mtk_lcm_create_input_packet(&input, 1, 0) < 0) {
			DDPPR_ERR("%s, failed to create input packet\n", __func__);
			return -ENOMEM;
		}

		/*init elvss input data*/
		ret = mtk_lcm_create_input(input.data, 1,
				MTK_LCM_INPUT_TYPE_MISC);
		if (ret < 0) {
			DDPPR_ERR("%s, %d failed to alloc elvss data\n", __func__, __LINE__);
			goto fail;
		}
		data = (u8 *)input.data->data;
		*data = (u8)((1<<7)|pulses);
	} else {
		DDPPR_ERR("%s, invalid flag:0x%x\n",
			__func__, bl_ext_config->cfg_flag);
		return -EINVAL;
	}

	DDPINFO("%s: %s,flag:0x%x,bl=%d,pulses:%d,mode:0x%x,update_bl:%d\n",
		__func__, owner, bl_ext_config->cfg_flag,
		bl_ext_config->backlight_level, pulses,
		ops->set_backlight_mask, update_bl);

	ret = mtk_panel_execute_callback_group(dsi, cb, handle,
			table, &input, owner);
	if (ret < 0) {
		DDPPR_ERR("%s, %d failed to %s, %d\n", __func__, __LINE__, owner, ret);
		goto end;
	}

	if (update_bl == true)
		atomic_set(&ctx_dsi->current_backlight, bl_ext_config->backlight_level);

end:
	mtk_lcm_destroy_input(input.data);
fail:
	mtk_lcm_destroy_input_packet(&input);

	return ret;
}

static int mtk_panel_set_value(int value)
{
	if (value == 1)
		atomic_set(&ctx_dsi->fake_mode, 1);
	else
		atomic_set(&ctx_dsi->fake_mode, 0);

	return 0;
}

static struct mtk_panel_funcs mtk_drm_panel_ext_funcs = {
	.set_backlight_cmdq = mtk_panel_set_backlight_cmdq,
	.set_aod_light_mode = mtk_panel_set_aod_light_mode,
	.set_backlight_grp_cmdq = mtk_panel_set_backlight_grp_cmdq,
	.reset = mtk_panel_reset,
	.ata_check = mtk_panel_ata_check,
	.ext_param_set = mtk_panel_ext_param_set,
	.ext_param_get = mtk_panel_ext_param_get,
	.mode_switch = mtk_panel_mode_switch,
	.get_virtual_heigh = mtk_panel_get_virtual_heigh,
	.get_virtual_width = mtk_panel_get_virtual_width,
	.doze_enable_start = mtk_panel_doze_enable_start,
	.doze_enable = mtk_panel_doze_enable,
	.doze_disable = mtk_panel_doze_disable,
	.doze_post_disp_on = mtk_panel_doze_post_disp_on,
	.doze_area = mtk_panel_doze_area,
	.doze_get_mode_flags = mtk_panel_doze_get_mode_flags,
	.hbm_set_cmdq = mtk_panel_hbm_set_cmdq,
	.hbm_get_state = mtk_panel_hbm_get_state,
	.hbm_get_wait_state = mtk_panel_hbm_get_wait_state,
	.hbm_set_wait_state = mtk_panel_hbm_set_wait_state,
	.lcm_dump = mtk_panel_dump,
	.msync_te_level_switch = mtk_panel_msync_te_level_switch,
	.msync_te_level_switch_grp = mtk_panel_msync_te_level_switch_grp,
	.msync_cmd_set_min_fps = mtk_panel_msync_cmd_set_min_fps,
	.get_lcm_version = mtk_panel_get_lcm_version,
	.ddic_ops = mtk_panel_ddic_ops,
	.cust_funcs = mtk_panel_cust_funcs,
	.read_panelid = mtk_panel_read_panelid,
	.set_bl_elvss_cmdq = mtk_panel_set_bl_elvss_cmdq,
	.set_value = mtk_panel_set_value,
};

static void mtk_drm_update_disp_mode_params(struct drm_display_mode *mode)
{
	struct mtk_lcm_params_dsi *params =
			&ctx_dsi->panel_resource->params.dsi_params;
	unsigned int fake_width = 0;
	unsigned int fake_heigh = 0;

	if (params->need_fake_resolution == 0)
		return;

	fake_width = params->fake_resolution[0];
	fake_heigh = params->fake_resolution[1];

	mode->vsync_start = fake_heigh +
			mode->vsync_start - mode->vdisplay;
	mode->vsync_end = fake_heigh +
			mode->vsync_end - mode->vdisplay;
	mode->vtotal = fake_heigh +
			mode->vtotal - mode->vdisplay;
	mode->vdisplay = fake_heigh;
	mode->hsync_start = fake_width +
			mode->hsync_start - mode->hdisplay;
	mode->hsync_end = fake_width +
			mode->hsync_end - mode->hdisplay;
	mode->htotal = fake_width +
			mode->htotal - mode->hdisplay;
	mode->hdisplay = fake_width;
}

static int mtk_drm_panel_get_modes(struct drm_panel *panel,
					struct drm_connector *connector)
{
	struct drm_display_mode *mode = NULL, *mode_src = NULL;
	struct mtk_lcm_params *params = NULL;
	struct mtk_lcm_params_dsi *dsi_params = NULL;
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	const struct mtk_panel_cust *cust = NULL;
	int count = 0, skip_fake = 0;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.get_modes != NULL)
		return cust->funcs.get_modes(panel, connector);

	if (IS_ERR_OR_NULL(connector)) {
		DDPPR_ERR("%s, invalid connect\n", __func__);
		return 0;
	}

	if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, %d invalid panel resource\n", __func__, __LINE__);
		return 0;
	}
	params = &ctx_dsi->panel_resource->params;
	dsi_params = &params->dsi_params;

	while (count < dsi_params->mode_count) {
		mode_node = mtk_drm_panel_get_mode_by_id(dsi_params, count);
		if (IS_ERR_OR_NULL(mode_node))
			break;

		if (atomic_read(&ctx_dsi->fake_mode) == 0 &&
			mode_node->fake > 0) {
			count++;
			skip_fake++;
			continue;
		}

		mode_src = &mode_node->mode;
		mtk_drm_update_disp_mode_params(mode_src);

		mode = drm_mode_duplicate(connector->dev, mode_src);
		if (IS_ERR_OR_NULL(mode)) {
			dev_err(connector->dev->dev,
				"failed to add mode %ux%ux@%u\n",
				mode_src->hdisplay, mode_src->vdisplay,
				drm_mode_vrefresh(mode_src));
			return count;
		}

		drm_mode_set_name(mode);
		mode->type = DRM_MODE_TYPE_DRIVER;
		if (mode_node->id == params->dsi_params.default_mode->id)
			mode->type |= DRM_MODE_TYPE_PREFERRED;

		drm_mode_probed_add(connector, mode);
		count++;
	}

	connector->display_info.width_mm =
			params->physical_width;
	connector->display_info.height_mm =
			params->physical_height;

	DDPMSG("%s- total count:%d, skip fake count:%d\n",
		__func__, count, skip_fake);
	return count;
}

static int mtk_drm_panel_get_timings(struct drm_panel *panel,
	unsigned int num_timings, struct display_timing *timings)
{
	const struct mtk_panel_cust *cust = NULL;

	if (IS_ERR_OR_NULL(ctx_dsi) ||
		IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	cust = ctx_dsi->panel_resource->cust;
	if (cust != NULL &&
		cust->funcs.get_timings != NULL)
		return cust->funcs.get_timings(panel,
				num_timings, timings);

	return 0;
}

static const struct drm_panel_funcs lcm_drm_funcs = {
	.disable = mtk_drm_panel_disable,
	.unprepare = mtk_drm_panel_unprepare,
	.prepare = mtk_drm_panel_prepare,
	.enable = mtk_drm_panel_enable,
	.get_modes = mtk_drm_panel_get_modes,
	.get_timings = mtk_drm_panel_get_timings,
};

static void check_is_need_fake_resolution(struct device *dev)
{
	struct mtk_lcm_params_dsi *params =
			&ctx_dsi->panel_resource->params.dsi_params;
	unsigned int fake_width = 0;
	unsigned int fake_heigh = 0;

	if (params->need_fake_resolution == 0)
		return;

	fake_width = params->fake_resolution[0];
	fake_heigh = params->fake_resolution[1];

	if (fake_heigh == 0 ||
		fake_heigh >= mtk_panel_get_virtual_heigh()) {
		DDPPR_ERR("%s: invalid fake heigh:%u\n",
			__func__, fake_heigh);
		params->need_fake_resolution = 0;
		return;
	}

	if (fake_width == 0 ||
		fake_width >= mtk_panel_get_virtual_width()) {
		DDPPR_ERR("%s: invalid fake width:%u\n",
			__func__, fake_width);
		params->need_fake_resolution = 0;
	}
}

static int mtk_drm_lcm_probe(struct mipi_dsi_device *dsi_dev)
{
	struct device *dev = &dsi_dev->dev;
	struct device_node *backlight = NULL, *lcm_np = NULL;
	struct mtk_lcm_params_dsi *dsi_params = NULL;
	unsigned int lcm_degree = 0;
	int ret = 0, probe_ret = 0;

	DDPMSG("%s++\n", __func__);
	ret = mtk_drm_lcm_dsi_init_ctx();
	if (ret < 0) {
		dev_err(dev, "%s, invalid ctx_dsi, panel resource\n",
			__func__);
		return ret;
	}

	lcm_np = of_parse_phandle(dev->of_node, "panel_cust", 0);
	if (lcm_np != NULL &&
		IS_ERR_OR_NULL(ctx_dsi->panel_resource->cust)) {
		DDPPR_ERR("%s, wait cust regist\n", __func__);
		return -EPROBE_DEFER;
	}
	ctx_dsi->dev = dev;

	lcm_np = of_parse_phandle(dev->of_node, "panel_resource", 0);
	if (IS_ERR_OR_NULL(lcm_np)) {
		DDPMSG("%s, %d, parse panel resource failed\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = load_panel_resource_from_dts(lcm_np,
			ctx_dsi->panel_resource);
	if (ret < 0) {
		DDPPR_ERR("%s, failed to load lcm resource from dts, ret:%d\n",
			__func__, ret);

		if (IS_ERR_OR_NULL(ctx_dsi->panel_resource) == 0) {
			free_lcm_resource(MTK_LCM_FUNC_DSI, ctx_dsi->panel_resource);
			ctx_dsi->panel_resource = NULL;
			DDPMSG("%s,%d, free panel resource, total_size:%lluByte\n",
				__func__, __LINE__, mtk_lcm_total_size);
		}

		return ret;
	}
	of_node_put(lcm_np);

	dsi_params = &ctx_dsi->panel_resource->params.dsi_params;
	dsi_dev->lanes = dsi_params->lanes;
	dsi_dev->format = dsi_params->format;
	dsi_dev->mode_flags = dsi_params->mode_flags;

	mtk_drm_gateic_select(ctx_dsi->panel_resource->params.name,
			MTK_LCM_FUNC_DSI);
	mipi_dsi_set_drvdata(dsi_dev, ctx_dsi);

	backlight = of_parse_phandle(dev->of_node, "backlight", 0);
	if (backlight) {
		ctx_dsi->backlight = of_find_backlight_by_node(backlight);
		of_node_put(backlight);

		if (!ctx_dsi->backlight) {
			DDPMSG("%s: delay by backlight node\n", __func__);
			return -EPROBE_DEFER;
		}
	}

	atomic_set(&ctx_dsi->prepared, 1);
	atomic_set(&ctx_dsi->enabled, 1);
	atomic_set(&ctx_dsi->error, 0);
	atomic_set(&ctx_dsi->hbm_en, 0);
	atomic_set(&ctx_dsi->current_backlight, 0xFF);
	atomic_set(&ctx_dsi->fake_mode, 0);
	atomic_set(&ctx_dsi->doze_enabled, 0);
	atomic_set(&ctx_dsi->doze_had, 0);
	atomic_set(&ctx_dsi->dim_status, 0);
	ctx_dsi->current_mode = dsi_params->default_mode;
	spin_lock_init(&ctx_dsi->lock);

	drm_panel_init(&ctx_dsi->panel, dev,
			&lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx_dsi->panel);

	ret = mipi_dsi_attach(dsi_dev);
	if (ret < 0) {
		DDPPR_ERR("%s: failed to attach dsi\n", __func__);
		drm_panel_remove(&ctx_dsi->panel);
	}

	mtk_panel_tch_handle_reg(&ctx_dsi->panel);

	ret = mtk_panel_ext_create(dev,
			&dsi_params->default_mode->ext_param,
			&mtk_drm_panel_ext_funcs, &ctx_dsi->panel);
	if (ret < 0) {
		DDPPR_ERR("%s, %d, creat ext failed, %d\n",
			__func__, __LINE__, ret);
		return ret;
	}
	check_is_need_fake_resolution(dev);
	mtk_lcm_support_cb = 0;

	probe_ret = of_property_read_u32(dev->of_node, "lcm-degree", &lcm_degree);
	if (probe_ret < 0)
		lcm_degree = 0;
	else
		dsi_params->default_mode->ext_param.lcm_degree = lcm_degree;

	DDPMSG("%s- cb:%u, degree:%u\n", __func__, mtk_lcm_support_cb,
		dsi_params->default_mode->ext_param.lcm_degree);

	return ret;
}

static int mtk_drm_lcm_remove(struct mipi_dsi_device *dsi_dev)
{
	//struct mtk_panel_context *ctx_dsi = mipi_dsi_get_drvdata(dsi_dev);

	DDPMSG("%s+\n", __func__);
	mipi_dsi_detach(dsi_dev);

	if (IS_ERR_OR_NULL(ctx_dsi))
		return 0;

	drm_panel_remove(&ctx_dsi->panel);
	mtk_drm_lcm_dsi_deinit_ctx();

	DDPMSG("%s-\n", __func__);
	return 0;
}

static const struct of_device_id mtk_panel_dsi_of_match[] = {
	{ .compatible = "mediatek,mtk-drm-panel-drv-dsi", },
	{ }
};

MODULE_DEVICE_TABLE(of, mtk_panel_dsi_of_match);

struct mipi_dsi_driver mtk_drm_panel_dsi_driver = {
	.probe = mtk_drm_lcm_probe,
	.remove = mtk_drm_lcm_remove,
	.driver = {
		.name = "mtk-drm-panel-drv-dsi",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mtk_panel_dsi_of_match),
	},
};

//module_mipi_dsi_driver(mtk_drm_panel_dsi_driver);

MODULE_AUTHOR("Cui Zhang <cui.zhang@mediatek.com>");
MODULE_DESCRIPTION("mediatek, drm panel dsi driver");
MODULE_LICENSE("GPL v2");
