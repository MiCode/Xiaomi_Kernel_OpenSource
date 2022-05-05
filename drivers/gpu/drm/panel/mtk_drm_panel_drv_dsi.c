// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_drm_panel_drv.h"

#ifdef CONFIG_MTK_ROUND_CORNER_SUPPORT
#include "../mediatek/mtk_corner_pattern/mtk_data_hw_roundedpattern.h"
#endif

#define MAX_PANEL_OPERATION_NAME (256)
#define MTK_DRM_PANEL_DEBUG

static struct mtk_panel_context *ctx_dsi;

static inline struct mtk_panel_context *panel_to_lcm(
		struct drm_panel *panel)
{
	return container_of(panel, struct mtk_panel_context, panel);
}

int mtk_panel_dsi_dcs_write_buffer(struct mipi_dsi_device *dsi,
		const void *data, size_t len)
{
	ssize_t ret;
	char *addr;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	addr = (char *)data;
	if ((int)*addr < 0xB0)
		ret = mipi_dsi_dcs_write_buffer(dsi, data, len);
	else
		ret = mipi_dsi_generic_write(dsi, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "error %zd writing seq: %ph\n",
			ret, data);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_write_buffer);

int mtk_panel_dsi_dcs_write(struct mipi_dsi_device *dsi,
		u8 cmd, void *data, size_t len)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_dcs_write(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev, "error %d write dcs cmd:(%#x)\n",
			ret, cmd);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_write);

int mtk_panel_dsi_dcs_read(struct mipi_dsi_device *dsi,
		u8 cmd, void *data, size_t len)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_dcs_read(dsi, cmd, data, len);
	if (ret < 0) {
		dev_err(ctx_dsi->dev,
			"error %d reading dcs cmd:(0x%x)\n", ret, cmd);
		atomic_set(&ctx_dsi->error, ret);
	}

	return ret;
}
EXPORT_SYMBOL(mtk_panel_dsi_dcs_read);

int mtk_panel_dsi_dcs_read_buffer(struct mipi_dsi_device *dsi,
		const void *data_in, size_t len_in,
		void *data_out, size_t len_out)
{
	ssize_t ret;

	if (atomic_read(&ctx_dsi->error) < 0)
		return 0;

	ret = mipi_dsi_generic_read(dsi, data_in,
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
int mtk_panel_register_dsi_customization_callback(
		struct mipi_dsi_device *dsi,
		struct mtk_panel_cust *cust)
{
	struct mtk_panel_cust *target_cust = NULL;
	struct device *dev = &dsi->dev;

	if (IS_ERR_OR_NULL(ctx_dsi)) {
		ctx_dsi = devm_kzalloc(dev,
				sizeof(struct mtk_panel_context),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(ctx_dsi))
			return -ENOMEM;
		ctx_dsi->dev = dev;
	} else if (ctx_dsi->dev != dev) {
		dev_err(dev, "%s, ctx_dsi device check failed\n",
			__func__);
	}

	if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		LCM_KZALLOC(ctx_dsi->panel_resource,
				sizeof(struct mtk_panel_resource), GFP_KERNEL);
		if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
			DDPPR_ERR("%s: failed to allocate panel resource\n", __func__);
			return -ENOMEM;
		}
	}

	target_cust = &ctx_dsi->panel_resource->cust;
	if (IS_ERR_OR_NULL(target_cust))
		DDPPR_ERR("%s: panel resource cust is null\n", __func__);

	if (atomic_read(&target_cust->cust_enabled) != 0) {
		DDPPR_ERR("%s %d: cust callback has already been registered\n",
			__func__, __LINE__);
		return -EINVAL;
	}

	target_cust->parse_params = cust->parse_params;
	target_cust->parse_ops = cust->parse_ops;
	target_cust->func = cust->func;
	target_cust->dump_params = cust->dump_params;
	target_cust->dump_ops = cust->dump_ops;
	atomic_set(&target_cust->cust_enabled, 1);
	DDPMSG("%s --\n", __func__);

	return 0;
}
EXPORT_SYMBOL(mtk_panel_register_dsi_customization_callback);

static int mtk_drm_panel_unprepare(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_params *params = &ctx_dsi->panel_resource->params;
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	int ret = 0;

	if (atomic_read(&ctx_dsi->prepared) == 0)
		return 0;

	DDPINFO("%s+\n", __func__);
	if (IS_ERR_OR_NULL(params) ||
	    IS_ERR_OR_NULL(ops))
		return -EINVAL;

	ret = mtk_panel_execute_operation((void *)dsi,
			&ops->unprepare, ctx_dsi->panel_resource,
			NULL, "panel_unprepare");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel unprepare, %d\n",
			__func__, __LINE__, ret);
		return ret;
	}

	atomic_set(&ctx_dsi->error, 0);
	atomic_set(&ctx_dsi->prepared, 0);
	atomic_set(&ctx_dsi->hbm_en, 0);
	ret = mtk_drm_gateic_power_off(MTK_LCM_FUNC_DSI);
	if (ret < 0)
		DDPPR_ERR("%s, gate ic power off failed, %d\n",
			__func__, ret);

	DDPMSG("%s- %d\n", __func__, ret);
	return ret;
}

static int mtk_drm_panel_do_prepare(struct mtk_panel_context *ctx_dsi)
{
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_input_packet input;
	unsigned long flags = 0;
	int ret = 0;

	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	/*prepare input data*/
	if (mtk_lcm_create_input_packet(&input, 1, 1) < 0)
		return -ENOMEM;

	if (mtk_lcm_create_input(input.condition, 1,
			MTK_LCM_INPUT_TYPE_CURRENT_FPS) < 0)
		goto fail2;
	spin_lock_irqsave(&ctx_dsi->lock, flags);
	*(unsigned int *)input.condition->data = ctx_dsi->current_mode->fps;
	spin_unlock_irqrestore(&ctx_dsi->lock, flags);

	if (mtk_lcm_create_input(input.data, 1,
			MTK_LCM_INPUT_TYPE_CURRENT_BACKLIGHT) < 0)
		goto fail1;
	*(u8 *)input.data->data = atomic_read(&ctx_dsi->current_backlight);

	/*do panel initialization*/
	ret = mtk_panel_execute_operation((void *)dsi,
			&ops->prepare, ctx_dsi->panel_resource,
			&input, "panel_prepare");

	if (ret < 0)
		DDPPR_ERR("%s,%d: failed to do panel prepare\n",
			__func__, __LINE__);

	/*deallocate input data*/
	mtk_lcm_destroy_input(input.data);
	input.data = NULL;
fail1:
	mtk_lcm_destroy_input(input.condition);
	input.condition = NULL;
fail2:
	mtk_lcm_destroy_input_packet(&input);
	return ret;
}

static int mtk_drm_panel_prepare(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	int ret = 0;

	if (atomic_read(&ctx_dsi->prepared) != 0)
		return 0;

	DDPINFO("%s+\n", __func__);
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

	atomic_set(&ctx_dsi->prepared, 1);

	mtk_panel_tch_rst(panel);

	DDPMSG("%s-, %d\n", __func__, ret);
	return ret;
}

static int mtk_drm_panel_enable(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	if (atomic_read(&ctx_dsi->enabled) != 0)
		return 0;

	DDPINFO("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	ret = mtk_panel_execute_operation((void *)dsi,
			&ops->enable, ctx_dsi->panel_resource,
			NULL, "panel_enable");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel enable\n",
			__func__, __LINE__);
		return ret;
	}

	if (ctx_dsi->backlight) {
		ctx_dsi->backlight->props.power = FB_BLANK_UNBLANK;
		backlight_update_status(ctx_dsi->backlight);
	}

	atomic_set(&ctx_dsi->enabled, 1);

	DDPMSG("%s-, %d\n", __func__, ret);
	return 0;
}

static int mtk_drm_panel_disable(struct drm_panel *panel)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	int ret = 0;

	if (atomic_read(&ctx_dsi->enabled) == 0)
		return 0;

	DDPINFO("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops))
		return -EINVAL;

	if (ctx_dsi->backlight) {
		ctx_dsi->backlight->props.power = FB_BLANK_POWERDOWN;
		backlight_update_status(ctx_dsi->backlight);
	}

	ret = mtk_panel_execute_operation((void *)dsi,
			&ops->disable, ctx_dsi->panel_resource,
			NULL, "panel_disable");

	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do panel disable\n",
			__func__, __LINE__);
		return ret;
	}

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
	struct mtk_lcm_params_dsi *params =
			&ctx_dsi->panel_resource->params.dsi_params;
	struct mtk_lcm_mode_dsi *mode_node;
	unsigned long flags = 0;
	bool found = false;
	struct drm_display_mode *mode = get_mode_by_connector_id(connector, id);

	DDPINFO("%s+\n", __func__);
	if (IS_ERR_OR_NULL(mode)) {
		DDPMSG("%s, failed to get mode\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(ext)) {
		DDPMSG("%s, failed to get ext\n", __func__);
		return -EINVAL;
	}

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
	struct mtk_lcm_mode_dsi *mode_node;
	struct mtk_lcm_params_dsi *params =
			&ctx_dsi->panel_resource->params.dsi_params;
	bool found = false;
	struct drm_display_mode *mode = NULL;

	mode = get_mode_by_connector_id(connector, id);
	if (IS_ERR_OR_NULL(mode)) {
		DDPMSG("%s, failed to get mode\n", __func__);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    params->mode_count == 0 ||
	    IS_ERR_OR_NULL(ext_param) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPPR_ERR("%s, invalid ctx, resource\n", __func__);
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
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	struct mtk_lcm_ops_input_packet input;
	u8 *data = NULL;
	int ret = 0, i = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops))
		return 0;

	/*prepare read back buffer*/
	if (mtk_lcm_create_input_packet(&input, 1, 0) < 0)
		return 0;

	if (mtk_lcm_create_input(input.data, ops->ata_id_value_length,
			MTK_LCM_INPUT_TYPE_READBACK) < 0)
		goto fail;

	ret = mtk_panel_execute_operation((void *)dsi,
			&ops->ata_check, ctx_dsi->panel_resource,
			&input, "ata_check");
	if (ret < 0) {
		DDPPR_ERR("%s,%d: failed to do ata check, %d\n",
			__func__, __LINE__, ret);
		ret = 0;
		goto end;
	}

	data = (u8 *)input.data->data;
	for (i = 0; i < ops->ata_id_value_length; i++) {
		if (data[i] != ops->ata_id_value_data[i]) {
			DDPPR_ERR("ATA data%d is expected:0x%x, we got:0x%x\n",
				i, ops->ata_id_value_data[i], data[i]);
			ret = 0;
			goto end;
		}
	}
	ret = 1; /*1 for pass*/

end:
	/*deallocate read back buffer*/
	mtk_lcm_destroy_input(input.data);
	input.data = NULL;
fail:
	mtk_lcm_destroy_input_packet(&input);

	DDPMSG("%s-, %d\n", __func__, ret);
	return ret;
}

static int panel_set_backlight(void *dsi, dcs_write_gce cb,
	void *handle, unsigned int level, struct mtk_lcm_ops_table *table,
	unsigned int mode)
{
	int ret = 0;
	unsigned int count = 0;
	u8 data[2] = { 0, 0 };

	if (IS_ERR_OR_NULL(table) || table->size == 0)
		return 0;

	if (level > 255)
		level = 255;

	if (mode <= 0xff) {
		count = 1;
		data[0] = level;
		data[1] = 0;
	} else if (mode <= 0xffff) {
		level = level * 4095 / 255;
		data[0] = ((level >> 8) & 0xf);
		data[1] = (level & 0xff);
		count = 2;
	} else
		count = 0;

	DDPINFO("%s, %d, mode:0x%x, level:0x%x, count:%u, data:0x%x, 0x%x\n",
		__func__, __LINE__, mode, level, count, data[0], data[1]);
	ret = mtk_panel_execute_callback(dsi, cb, handle,
				data, count, table, "set_backlight");
	atomic_set(&ctx_dsi->current_backlight, level);

	return ret;
}

static int panel_set_backlight_group(void *dsi, dcs_grp_write_gce cb,
	void *handle, unsigned int level, struct mtk_lcm_ops_table *table,
	unsigned int mode)
{
	int ret = 0;
	unsigned int count = 0;
	u8 data[2] = { 0, 0 };

	if (IS_ERR_OR_NULL(table) || table->size == 0)
		return 0;

	if (level > 255)
		level = 255;

	if (mode <= 0xff) {
		count = 1;
		data[0] = level;
		data[1] = 0;
	} else if (mode <= 0xffff) {
		level = level * 4095 / 255;
		data[0] = ((level >> 8) & 0xf);
		data[1] = (level & 0xff);
		count = 2;
	} else
		count = 0;

	DDPINFO("%s, %d, mode:0x%x, level:0x%x, count:%u, data:0x%x, 0x%x\n",
		__func__, __LINE__, mode, level, count, data[0], data[1]);
	ret = mtk_panel_execute_callback_group(dsi, cb, handle,
				data, count, table, "set_backlight");
	atomic_set(&ctx_dsi->current_backlight, level);

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

	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource->ops.dsi_ops)) {
		DDPMSG("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
	    ops->set_backlight_cmdq.size == 0) {
		DDPMSG("%s, invalid backlight table\n", __func__);
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

	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource->ops.dsi_ops)) {
		DDPMSG("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
	    ops->set_backlight_cmdq.size == 0) {
		DDPMSG("%s, invalid backlight table\n", __func__);
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
	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource))
		return 0;

	return ctx_dsi->panel_resource->params.resolution[1];
}

static int mtk_panel_get_virtual_width(void)
{
	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource))
		return 0;

	return ctx_dsi->panel_resource->params.resolution[0];
}

static int mtk_panel_mode_switch(struct drm_panel *panel,
		struct drm_connector *connector,
		unsigned int cur_mode, unsigned int dst_mode,
		enum MTK_PANEL_MODE_SWITCH_STAGE stage)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_table *table = NULL;
	struct mtk_lcm_params *params = &ctx_dsi->panel_resource->params;
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	struct mipi_dsi_device *dsi = to_mipi_dsi_device(ctx_dsi->dev);
	struct drm_display_mode *mode = NULL;
	char owner[MAX_PANEL_OPERATION_NAME] = {0};
	struct mtk_lcm_mode_dsi *mode_node;
	bool found = false;
	int ret = 0;

	if (cur_mode == dst_mode)
		return ret;

	DDPINFO("%s+, cur:%u, dst:%u, stage:%d, powerdown:%d\n",
		__func__, cur_mode, dst_mode, stage, BEFORE_DSI_POWERDOWN);
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
		table = &mode_node->fps_switch_bfoff;
		break;
	case AFTER_DSI_POWERON:
		table = &mode_node->fps_switch_afon;
		break;
	default:
		DDPPR_ERR("%s: invalid stage:%d\n", __func__, stage);
		return -EINVAL;
	}

	ret = snprintf(owner, sizeof(owner), "fps-switch-%u-%u-%u",
		mode_node->width, mode_node->height, mode_node->fps);
	if (ret < 0 || (size_t)ret >= sizeof(owner))
		DDPMSG("%s, %d, snprintf failed\n", __func__, __LINE__);
	ret = mtk_panel_execute_operation((void *)dsi, table,
				ctx_dsi->panel_resource,
				NULL, owner);

	DDPMSG("%s-, %d\n", __func__, ret);
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

	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource->ops.dsi_ops)) {
		DDPMSG("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
	    ops->set_aod_light.size == 0) {
		DDPMSG("%s, invalid aod light mod table\n", __func__);
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
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops) ||
	    ops->doze_enable_start.size == 0)
		return -EINVAL;

	ret = mtk_panel_execute_callback(dsi, cb, handle,
			NULL, 0, &ops->doze_enable_start, "doze_enable_start");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_enable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops) ||
	    ops->doze_enable.size == 0)
		return -EINVAL;

	ret = mtk_panel_execute_callback(dsi, cb, handle,
			NULL, 0, &ops->doze_enable, "doze_enable");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_disable(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops) ||
	    ops->doze_disable.size == 0)
		return -EINVAL;

	ret = mtk_panel_execute_callback(dsi, cb, handle,
			NULL, 0, &ops->doze_disable, "doze_disable");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_post_disp_on(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops) ||
	    ops->doze_post_disp_on.size == 0)
		return -EINVAL;

	ret = mtk_panel_execute_callback(dsi, cb, handle,
			NULL, 0, &ops->doze_post_disp_on, "doze_post_disp_on");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static int mtk_panel_doze_area(struct drm_panel *panel,
	void *dsi, dcs_write_gce cb, void *handle)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ops) ||
	    ops->doze_area.size == 0)
		return -EINVAL;

	ret = mtk_panel_execute_callback(dsi, cb, handle,
			NULL, 0, &ops->doze_area, "doze_area");
	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static unsigned long mtk_panel_doze_get_mode_flags(
	struct drm_panel *panel, int doze_en)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_lcm_params *params = &ctx_dsi->panel_resource->params;

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
	u8 data = 0;
	int ret = 0;

	DDPMSG("%s+\n", __func__);
	if (IS_ERR_OR_NULL(ctx_dsi) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource) ||
	    IS_ERR_OR_NULL(ctx_dsi->panel_resource->ops.dsi_ops)) {
		DDPMSG("%s, invalid ctx or panel resource\n", __func__);
		return -EINVAL;
	}

	ops = ctx_dsi->panel_resource->ops.dsi_ops;
	if (IS_ERR_OR_NULL(ops) ||
	    ops->hbm_set_cmdq.size == 0)
		return -EINVAL;

	if (atomic_read(&ctx_dsi->hbm_en) == en)
		return 0;

	if (en)
		data = ops->hbm_set_cmdq_switch_on;
	else
		data = ops->hbm_set_cmdq_switch_off;
	ret = mtk_panel_execute_callback(dsi, cb, handle,
			&data, 1, &ops->hbm_set_cmdq, "hbm_set_cmdq");
	atomic_set(&ctx_dsi->hbm_en, en);
	atomic_set(&ctx_dsi->hbm_wait, 1);

	DDPMSG("%s- ret:%d\n", __func__, ret);
	return ret;
}

static void mtk_panel_hbm_get_state(struct drm_panel *panel, bool *state)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);

	*state = atomic_read(&ctx_dsi->hbm_en);
}

static void mtk_panel_hbm_get_wait_state(struct drm_panel *panel, bool *wait)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);

	*wait = atomic_read(&ctx_dsi->hbm_wait);
}

static bool mtk_panel_hbm_set_wait_state(struct drm_panel *panel, bool wait)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	bool old = atomic_read(&ctx_dsi->hbm_wait);

	atomic_set(&ctx_dsi->hbm_wait, wait);
	return old;
}

static void mtk_panel_dump(struct drm_panel *panel, enum MTK_LCM_DUMP_FLAG flag)
{
	struct mtk_panel_context *ctx_dsi = panel_to_lcm(panel);
	struct mtk_panel_resource *resource = ctx_dsi->panel_resource;

	switch (flag) {
	case MTK_DRM_PANEL_DUMP_PARAMS:
		dump_lcm_params_basic(&resource->params);
		dump_lcm_params_dsi(&resource->params.dsi_params, &resource->cust);
		break;
	case MTK_DRM_PANEL_DUMP_OPS:
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, &resource->cust);
		break;
	case MTK_DRM_PANEL_DUMP_ALL:
		dump_lcm_params_basic(&resource->params);
		dump_lcm_params_dsi(&resource->params.dsi_params, &resource->cust);
		dump_lcm_ops_dsi(resource->ops.dsi_ops,
				&resource->params.dsi_params, &resource->cust);
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
	struct mtk_lcm_mode_dsi *max_node = NULL;
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
	DDPMSG("%s, %d, level:%u, max_fps:%u\n",
		__func__, __LINE__, level, max_fps);

	return max_node;
}

static struct mtk_lcm_msync_min_fps_switch *mtk_lcm_find_1st_min_fps_switch(
	unsigned int level, struct mtk_lcm_mode_dsi *mode_node)
{
	struct mtk_lcm_msync_min_fps_switch *node = NULL;
	struct mtk_lcm_msync_min_fps_switch *min_node = NULL;
	unsigned int min_fps = 0;

	list_for_each_entry(node, &mode_node->msync_min_fps_switch, list) {
		if (node->fps <= level &&
		    node->fps > min_fps) {
			min_fps = node->fps;
			min_node = node;
		}
	}
	DDPMSG("%s, %d, level:%u, min_fps:%u\n",
		__func__, __LINE__, level, min_fps);

	return node;
}

#define MTK_LCM_MTE_OFF (0xFFFF)
static int mtk_panel_msync_te_level_switch(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int fps_level)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	int ret = 0;

	DDPMSG("%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);

	if (fps_level == MTK_LCM_MTE_OFF) { /*close multi te */
		struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;

		DDPMSG("%s:%d Close MTE\n", __func__, __LINE__);
		if (IS_ERR_OR_NULL(ops) ||
		    ops->msync_close_mte.size == 0 ||
		    ops->msync_default_mte.size == 0) {
			DDPMSG("%s, %d, invalid msync operation\n", __func__, __LINE__);
			ret = -EINVAL;
			goto out;
		}

		ret = mtk_panel_execute_callback(dsi, cb, handle,
				NULL, 0, &ops->msync_close_mte, "msync_close_mte");
		ret = mtk_panel_execute_callback(dsi, cb, handle,
				NULL, 0, &ops->msync_default_mte, "msync_default_mte");
	} else {
		mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
		if (mode_node != NULL) {
			DDPMSG("%s:%d switch to fps:%u, level:%u\n",
				__func__, __LINE__, mode_node->fps, fps_level);
			ret = mtk_panel_execute_callback(dsi, cb, handle,
					NULL, 0, &mode_node->msync_switch_mte, "msync_switch_mte");
		} else {
			DDPMSG("%s, %d, failed to find fps node of level:%u\n",
				__func__, __LINE__, fps_level);
			ret = 1;
			goto out;
		}
	}

out:
	DDPMSG("%s:%d fps_level:%d, ret:%%u\n",
		__func__, __LINE__, fps_level, ret);
	return ret;
}

static int mtk_panel_msync_te_level_switch_grp(void *dsi, dcs_grp_write_gce cb,
		void *handle, struct drm_panel *panel, unsigned int fps_level)
{
	struct mtk_lcm_mode_dsi *mode_node = NULL;
	int ret = 0;

	DDPMSG("%s:%d fps_level:%d\n", __func__, __LINE__, fps_level);

	if (fps_level == MTK_LCM_MTE_OFF) { /*close multi te */
		struct mtk_lcm_ops_dsi *ops = ctx_dsi->panel_resource->ops.dsi_ops;

		DDPMSG("%s:%d Close MTE\n", __func__, __LINE__);
		if (IS_ERR_OR_NULL(ops) ||
		    ops->msync_close_mte.size == 0 ||
		    ops->msync_default_mte.size == 0) {
			DDPMSG("%s, %d, invalid msync operation\n", __func__, __LINE__);
			ret = -EINVAL;
			goto out;
		}

		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
				NULL, 0, &ops->msync_close_mte, "msync_close_mte");
		ret = mtk_panel_execute_callback_group(dsi, cb, handle,
				NULL, 0, &ops->msync_default_mte, "msync_default_mte");
	} else {
		mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
		if (mode_node != NULL) {
			DDPMSG("%s:%d switch to fps:%u, level:%u\n",
				__func__, __LINE__, mode_node->fps, fps_level);
			ret = mtk_panel_execute_callback_group(dsi, cb, handle,
					NULL, 0, &mode_node->msync_switch_mte, "msync_switch_mte");
		} else {
			DDPPR_ERR("%s, %d, failed to find max fps mode, level:%u\n",
				__func__, __LINE__, fps_level);
			ret = 1;
			goto out;
		}
	}

out:
	DDPMSG("%s:%d fps_level:%d, ret:%%u\n",
		__func__, __LINE__, fps_level, ret);
	return ret;
}

static int mtk_panel_msync_cmd_set_min_fps(void *dsi, dcs_write_gce cb,
		void *handle, unsigned int flag)
{
	unsigned int fps_level = (flag & 0xFFFF0000) >> 16;
	unsigned int min_fps = flag & 0xFFFF;
	struct mtk_lcm_mode_dsi *mode_node;
	struct mtk_lcm_ops_dsi *ops =
			ctx_dsi->panel_resource->ops.dsi_ops;
	int ret = 0;

	DDPMSG("%s:%d flag:0x%08x, fps_level:%u min_fps:%u\n",
			__func__, __LINE__, flag, fps_level, min_fps);
	mode_node = mtk_lcm_find_1st_max_fps_mode(fps_level);
	if (mode_node != NULL) {
		struct mtk_lcm_msync_min_fps_switch *node = NULL;

		DDPMSG("%s:%d get max fps mode:%u, level:%u\n",
			__func__, __LINE__, mode_node->fps, fps_level);
		node = mtk_lcm_find_1st_min_fps_switch(min_fps, mode_node);
		if (node != NULL) {
			DDPMSG("%s:%d get min fps switch:%u, level:%u\n",
				__func__, __LINE__, node->fps, min_fps);
			ret = mtk_panel_execute_callback(dsi, cb, handle,
				node->data, node->count,
				&ops->msync_set_min_fps, "msync_set_min_fps");
		} else {
			DDPPR_ERR("%s, %d, failed to get min fps switch, level:%u\n",
				__func__, __LINE__, min_fps);
			ret = -EFAULT;
		}
	} else {
		DDPPR_ERR("%s, %d, failed to get max fps mode, level:%u\n",
			__func__, __LINE__, fps_level);
		ret = -EFAULT;
	}

	return ret;
}

static enum mtk_lcm_version mtk_panel_get_lcm_version(void)
{
	return MTK_COMMON_LCM_DRV;
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
	int count = 0;

	if (IS_ERR_OR_NULL(connector)) {
		DDPMSG("%s, invalid connect\n", __func__);
		return 0;
	}

	if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		DDPMSG("%s, %d invalid panel resource\n", __func__, __LINE__);
		return 0;
	}
	params = &ctx_dsi->panel_resource->params;
	dsi_params = &params->dsi_params;

	while (count < dsi_params->mode_count) {
		mode_node = mtk_drm_panel_get_mode_by_id(dsi_params, count);
		if (IS_ERR_OR_NULL(mode_node))
			break;

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

	DDPMSG("%s- count =%d\n", __func__, count);
	return count;
}

static int mtk_drm_panel_get_timings(struct drm_panel *panel,
	unsigned int num_timings, struct display_timing *timings)
{
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

static int mtk_drm_lcm_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct device_node *backlight = NULL, *lcm_np = NULL;
	struct mtk_lcm_params_dsi *dsi_params = NULL;
	int ret = 0;

	DDPMSG("%s++\n", __func__);
	if (IS_ERR_OR_NULL(ctx_dsi)) {
		ctx_dsi = devm_kzalloc(dev,
				sizeof(struct mtk_panel_context),
				GFP_KERNEL);
		if (IS_ERR_OR_NULL(ctx_dsi)) {
			DDPMSG("%s, %d, failed to allocate buffer\n", __func__, __LINE__);
			return -ENOMEM;
		}
		ctx_dsi->dev = dev;
	} else if (ctx_dsi->dev != dev) {
		dev_err(dev, "%s, ctx_dsi device check failed\n",
			__func__);
	}

	if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
		LCM_KZALLOC(ctx_dsi->panel_resource,
			sizeof(struct mtk_panel_resource), GFP_KERNEL);
		if (IS_ERR_OR_NULL(ctx_dsi->panel_resource)) {
			DDPPR_ERR("%s: failed to allocate panel resource\n", __func__);
			return -ENOMEM;
		}
	}

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
	dsi->lanes = dsi_params->lanes;
	dsi->format = dsi_params->format;
	dsi->mode_flags = dsi_params->mode_flags;

	mtk_drm_gateic_select(ctx_dsi->panel_resource->params.name,
			MTK_LCM_FUNC_DSI);
	mipi_dsi_set_drvdata(dsi, ctx_dsi);

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
	ctx_dsi->current_mode = dsi_params->default_mode;
	spin_lock_init(&ctx_dsi->lock);

	drm_panel_init(&ctx_dsi->panel, dev,
			&lcm_drm_funcs, DRM_MODE_CONNECTOR_DSI);
	drm_panel_add(&ctx_dsi->panel);

	ret = mipi_dsi_attach(dsi);
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
	DDPMSG("%s-\n", __func__);

	return ret;
}

static int mtk_drm_lcm_remove(struct mipi_dsi_device *dsi)
{
	//struct mtk_panel_context *ctx_dsi = mipi_dsi_get_drvdata(dsi);

	DDPMSG("%s+\n", __func__);
	mipi_dsi_detach(dsi);

	if (IS_ERR_OR_NULL(ctx_dsi))
		return 0;

	drm_panel_remove(&ctx_dsi->panel);

	if (ctx_dsi->panel_resource != NULL) {
		free_lcm_resource(MTK_LCM_FUNC_DSI, ctx_dsi->panel_resource);
		ctx_dsi->panel_resource = NULL;
		DDPMSG("%s,%d, free panel resource, total_size:%lluByte\n",
			__func__, __LINE__, mtk_lcm_total_size);
	}

	if (ctx_dsi->dev != NULL) {
		devm_kfree(ctx_dsi->dev, ctx_dsi);
		ctx_dsi->dev = NULL;
		ctx_dsi = NULL;
	}

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
