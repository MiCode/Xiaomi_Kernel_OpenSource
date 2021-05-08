// SPDX-License-Identifier: GPL-2.0
/*
 * mddp_if.c - Interface API between MDDP and other kernel module.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/netdevice.h>

#include "mddp_ctrl.h"
#include "mddp_debug.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_sm.h"
#include "mddp_usage.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------
#define MDDP_CHECK_APP_TYPE(_type) \
	((_type >= 0 && _type < MDDP_APP_TYPE_CNT) ? (1) : (0))

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_drv_attach(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle)
{
	if (MDDP_CHECK_APP_TYPE(conf->app_type) && handle)
		return mddp_sm_reg_callback(conf, handle);

	MDDP_C_LOG(MDDP_LL_WARN,
			"%s: Failed to drv_attach, type(%d), handle(%p)!\n",
			__func__, conf->app_type, handle);

	return -EINVAL;
}
EXPORT_SYMBOL(mddp_drv_attach);

void mddp_drv_detach(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle)
{
	if (MDDP_CHECK_APP_TYPE(conf->app_type))
		mddp_sm_dereg_callback(conf, handle);
}
EXPORT_SYMBOL(mddp_drv_detach);

int32_t mddp_on_enable(enum mddp_app_type_e in_type)
{
	struct mddp_app_t      *app;
	uint32_t                type;
	uint8_t                 idx;

	if (in_type != MDDP_APP_TYPE_ALL)
		return -EINVAL;

	/*
	 * MDDP ENABLE command.
	 */
	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);
		mddp_sm_on_event(app, MDDP_EVT_FUNC_ENABLE);
	}

	return 0;
}

int32_t mddp_on_disable(enum mddp_app_type_e in_type)
{
	struct mddp_app_t      *app;
	uint32_t                type;
	uint8_t                 idx;

	if (in_type != MDDP_APP_TYPE_ALL)
		return -EINVAL;

	/*
	 * MDDP DISABLE command.
	 */
	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);
		mddp_sm_on_event(app, MDDP_EVT_FUNC_DISABLE);
	}

	return 0;
}

int32_t mddp_on_activate(enum mddp_app_type_e type,
		uint8_t *ul_dev_name, uint8_t *dl_dev_name)
{
	struct mddp_app_t      *app;

	// NG. app_type is unknown!
	if (!MDDP_CHECK_APP_TYPE(type))
		return -EINVAL;

	// NG. app is not configured!
	app = mddp_get_app_inst(type);
	if (!app->is_config)
		return -EINVAL;

	/*
	 * MDDP ACTIVATE command.
	 */
	strlcpy(app->ap_cfg.ul_dev_name, ul_dev_name,
			sizeof(app->ap_cfg.ul_dev_name));
	strlcpy(app->ap_cfg.dl_dev_name, dl_dev_name,
			sizeof(app->ap_cfg.dl_dev_name));
	MDDP_C_LOG(MDDP_LL_INFO,
			"%s: type(%d), app(%p), ul(%s), dl(%s).\n",
			__func__, type, app,
			app->ap_cfg.ul_dev_name, app->ap_cfg.dl_dev_name);
	mddp_sm_on_event(app, MDDP_EVT_FUNC_ACT);
	mddp_u_set_wan_iface(ul_dev_name);

	return 0;
}

int32_t mddp_on_deactivate(enum mddp_app_type_e type)
{
	struct mddp_app_t      *app;

	// NG. app_type is unknown!
	if (!MDDP_CHECK_APP_TYPE(type))
		return -EINVAL;

	// NG. app is not configured!
	app = mddp_get_app_inst(type);
	if (!app->is_config)
		return -EINVAL;

	/*
	 * MDDP DEACTIVATE command.
	 */
	mddp_sm_on_event(app, MDDP_EVT_FUNC_DEACT);

	return 0;
}

int32_t mddp_on_get_offload_stats(
		enum mddp_app_type_e type,
		uint8_t *buf,
		uint32_t *buf_len)
{
	if (type != MDDP_APP_TYPE_ALL)
		return -EINVAL;

	/*
	 * MDDP GET_OFFLOAD_STATISTICS command.
	 */
	mddp_u_get_data_stats(buf, buf_len);

	return 0;
}

int32_t mddp_on_set_data_limit(
		enum mddp_app_type_e type,
		uint8_t *buf,
		uint32_t buf_len)
{
	int32_t                 ret;

	if (type != MDDP_APP_TYPE_ALL)
		return -EINVAL;

	/*
	 * MDDP GET_OFFLOAD_STATISTICS command.
	 */
	ret = mddp_u_set_data_limit(buf, buf_len);

	return ret;
}

int32_t mddp_on_set_ct_value(
		enum mddp_app_type_e type,
		uint8_t *buf,
		uint32_t buf_len)
{
	int32_t                 ret;

	if (type != MDDP_APP_TYPE_ALL)
		return -EINVAL;

	/*
	 * MDDP GET_OFFLOAD_STATISTICS command.
	 */
	ret = mddp_f_set_ct_value(buf, buf_len);

	return ret;
}

//------------------------------------------------------------------------------
// Kernel functions.
//------------------------------------------------------------------------------
static int __init mddp_init(void)
{
	int32_t         ret = 0;

	ret = mddp_sm_init();
	if (ret < 0)
		goto _init_fail;

	ret = mddp_ipc_init();
	if (ret < 0)
		goto _init_fail;

	ret = mddp_dev_init();
	if (ret < 0)
		goto _init_fail;

	ret = mddp_filter_init();
	if (ret < 0)
		goto _init_fail;

	ret = mddp_usage_init();
	if (ret < 0)
		goto _init_fail;


_init_fail:
	return ret;
}

static void __exit mddp_exit(void)
{
	synchronize_net();

	mddp_usage_uninit();
	mddp_filter_uninit();
	mddp_dev_uninit();
	mddp_ipc_uninit();
	mddp_sm_uninit();
}
module_init(mddp_init);
module_exit(mddp_exit);

MODULE_LICENSE("GPL v2");
