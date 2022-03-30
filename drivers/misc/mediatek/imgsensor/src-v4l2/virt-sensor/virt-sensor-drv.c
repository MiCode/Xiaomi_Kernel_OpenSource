// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2020 MediaTek Inc.

#include "adaptor.h"
#include "virt-sensor-entry.h"
#include "../imgsensor-glue/imgsensor-glue.h"


#define ctx_to_target(ctx) \
	container_of(container_of(ctx, struct adaptor_ctx, subctx)->subdrv, \
		struct external_entry, wrapper)->target

#define ctx_to_adaptor(ctx) \
	container_of(ctx, struct adaptor_ctx, subctx)

#define call_target_ops(ctx, o, args...) \
({ \
	struct subdrv_entry *__target = ctx_to_target(ctx); \
	int __ret; \
	if (!__target || !__target->ops) \
		__ret = -ENODEV; \
	else if (!__target->ops->o) \
		__ret = -ENOIOCTLCMD; \
	else \
		__ret = __target->ops->o(ctx, ##args); \
	__ret; \
})


static int wrapper_get_id(struct subdrv_ctx *ctx, u32 *id)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, get_id, id);

	return ret;
}

static int wrapper_init_ctx(struct subdrv_ctx *ctx,
			struct i2c_client *i2c_client, u8 i2c_write_id)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, init_ctx, i2c_client, i2c_write_id);

	return ret;
}

static int wrapper_open(struct subdrv_ctx *ctx)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, open);

	return ret;
}

static int wrapper_get_info(struct subdrv_ctx *ctx,
			enum MSDK_SCENARIO_ID_ENUM scenario_id,
			MSDK_SENSOR_INFO_STRUCT *pSensorInfo,
			MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, get_info, scenario_id, pSensorInfo, pSensorConfigData);

	return ret;
}

static int wrapper_get_resolution(struct subdrv_ctx *ctx,
			MSDK_SENSOR_RESOLUTION_INFO_STRUCT *pSensorResolution)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, get_resolution, pSensorResolution);

	return ret;
}

static int wrapper_control(struct subdrv_ctx *ctx,
			enum MSDK_SCENARIO_ID_ENUM ScenarioId,
			MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *pImageWindow,
			MSDK_SENSOR_CONFIG_STRUCT *pSensorConfigData)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, control, ScenarioId, pImageWindow, pSensorConfigData);

	return ret;
}

static int wrapper_feature_control(struct subdrv_ctx *ctx,
			MSDK_SENSOR_FEATURE_ENUM FeatureId,
			MUINT8 *pFeaturePara,
			MUINT32 *pFeatureParaLen)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, feature_control, FeatureId, pFeaturePara, pFeatureParaLen);

	return ret;
}

static int wrapper_close(struct subdrv_ctx *ctx)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, close);

	return ret;
}

#ifdef IMGSENSOR_VC_ROUTING
static int wrapper_get_frame_desc(struct subdrv_ctx *ctx,
			int scenario_id,
			struct mtk_mbus_frame_desc *fd)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, get_frame_desc, scenario_id, fd);

	return ret;
}
#endif

static int wrapper_get_temp(struct subdrv_ctx *ctx, int *temp)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, get_temp, temp);

	return ret;
}

static int wrapper_vsync_notify(struct subdrv_ctx *ctx, unsigned int sof_cnt)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, vsync_notify, sof_cnt);

	return ret;
}

static int wrapper_power_on(struct subdrv_ctx *ctx, void *data)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, power_on, data);

	return ret;
}

static int wrapper_power_off(struct subdrv_ctx *ctx, void *data)
{
	int ret = 0;

	dev_info(ctx_to_adaptor(ctx)->dev,
		"[%s] %s", __func__, ctx_to_target(ctx)->name);

	ret = call_target_ops(ctx, power_off, data);

	return ret;
}

static struct subdrv_ops def_ops = {
	.get_id = wrapper_get_id,
	.init_ctx = wrapper_init_ctx,
	.open = wrapper_open,
	.get_info = wrapper_get_info,
	.get_resolution = wrapper_get_resolution,
	.control = wrapper_control,
	.feature_control = wrapper_feature_control,
	.close = wrapper_close,
#ifdef IMGSENSOR_VC_ROUTING
	.get_frame_desc = wrapper_get_frame_desc,
#endif
	.get_temp = wrapper_get_temp,
	.vsync_notify = wrapper_vsync_notify,

	.power_on = wrapper_power_on,
	.power_off = wrapper_power_off,
};

static void init_wrapper(struct external_entry *entry)
{

	if (entry) {
		entry->wrapper.name = entry->target->name;
		entry->wrapper.id = entry->target->id;
		entry->wrapper.pw_seq = entry->target->pw_seq;
		entry->wrapper.pw_seq_cnt = entry->target->pw_seq_cnt;

		entry->wrapper.ops = &def_ops;
	}
}

struct subdrv_entry *vs_query_subdrv_entry(const char *name)
{
	struct external_entry *entry = NULL;

	pr_info("[%s] searching %s", __func__, name);

	entry = query_external_subdrv_entry(name);

	if (entry) {
		init_wrapper(entry);
		return &entry->wrapper;
	}

	return NULL;
}

