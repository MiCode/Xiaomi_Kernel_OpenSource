/*
 * mddp_if.c - Interface API between MDDP and other kernel module.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/types.h>
#include <linux/module.h>
#include <linux/skbuff.h>

#include "mddp_ctrl.h"
#include "mddp_dev.h"
#include "mddp_filter.h"
#include "mddp_ipc.h"
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
static uint8_t mddp_md_version_s = 0xff;

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
int32_t mddp_md_msg_hdlr(void *in_ilm)
{
	int32_t          ret = 0;
	struct ipc_ilm  *ilm;

	ilm = (struct ipc_ilm *)in_ilm;

	switch (ilm->msg_id) {
	case IPC_MSG_ID_UFPM_ENABLE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_UFPM_DISABLE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_UFPM_ACTIVATE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_UFPM_DEACTIVATE_MD_FAST_PATH_IND:
	case IPC_MSG_ID_UFPM_SEND_MD_USB_EP0_RSP:
	case IPC_MSG_ID_UFPM_SEND_AP_USB_EP0_IND:
		ret = mddp_sm_msg_hdlr(MDDP_APP_TYPE_USB, ilm);
		break;
#if defined CONFIG_MTK_MDDP_WH_SUPPORT || defined CONFIG_MTK_MCIF_WIFI_SUPPORT
	case IPC_MSG_ID_WFPM_ENABLE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_WFPM_DISABLE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_RSP:
	case IPC_MSG_ID_WFPM_DEACTIVATE_MD_FAST_PATH_IND:
	case IPC_MSG_ID_WFPM_ACTIVATE_MD_FAST_PATH_RSP:
		ret = mddp_sm_msg_hdlr(MDDP_APP_TYPE_WH, ilm);
		break;
#endif
	case IPC_MSG_ID_MDT_DATA_USAGE_CMD:
		ret = mddp_u_msg_hdlr();
		break;

	default:
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(mddp_md_msg_hdlr);

int32_t mddp_drv_attach(
	struct mddp_drv_conf_t *conf,
	struct mddp_drv_handle_t *handle)
{
	if (MDDP_CHECK_APP_TYPE(conf->app_type) && handle)
		return mddp_sm_reg_callback(conf, handle);

	pr_notice("%s: Failed to drv_attach, type(%d), handle(%p)!\n",
				__func__, conf->app_type, handle);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mddp_drv_attach);

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
	uint8_t                 idx;

	if (!MDDP_CHECK_APP_TYPE(type))
		return -EINVAL;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		if (type == mddp_sm_module_list_s[idx])
			break;
	}

	if (idx == MDDP_MOD_CNT)
		return -EINVAL;
	/*
	 * MDDP ACTIVATE command.
	 */
	app = mddp_get_app_inst(type);
	memcpy(&app->ap_cfg.ul_dev_name, ul_dev_name, strlen(ul_dev_name));
	memcpy(&app->ap_cfg.dl_dev_name, dl_dev_name, strlen(dl_dev_name));
	pr_info("%s: %d, type(%d), app(%p), ul(%s), dl(%s).\n",
			__func__, 3, type, app,
			app->ap_cfg.ul_dev_name, app->ap_cfg.dl_dev_name);
	mddp_sm_on_event(app, MDDP_EVT_FUNC_ACT);

	return 0;
}

int32_t mddp_on_deactivate(enum mddp_app_type_e type)
{
	struct mddp_app_t      *app;
	uint8_t                 idx;

	if (!MDDP_CHECK_APP_TYPE(type))
		return -EINVAL;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		if (type == mddp_sm_module_list_s[idx])
			break;
	}

	if (idx == MDDP_MOD_CNT)
		return -EINVAL;
	/*
	 * MDDP DEACTIVATE command.
	 */
	app = mddp_get_app_inst(type);
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

int32_t mddp_send_msg_to_md_isr(enum mddp_app_type_e type,
		uint32_t msg_id, void *data, uint32_t data_len)
{
	struct mddp_app_t      *app;
	struct mddp_md_queue_t *md_queue;
	struct mddp_md_msg_t   *md_msg;
	unsigned long           flags;

	app = mddp_get_app_inst(type);
	md_queue = &app->md_send_queue;
	if (unlikely(!(app->is_config) || !md_queue)) {
		pr_notice("%s: Invalid state, config(%d), queue(%p)!\n",
				__func__, app->is_config, md_queue);
		WARN_ON(1);
		return -EPERM;
	}

	md_msg = kzalloc(sizeof(struct mddp_md_msg_t) + data_len, GFP_ATOMIC);
	if (unlikely(!md_msg))
		return -ENOMEM;

	md_msg->msg_id = msg_id;
	md_msg->data_len = data_len;
	memcpy(md_msg->data, data, data_len);

	spin_lock_irqsave(&md_queue->locker, flags);
	list_add_tail(&md_msg->list, &md_queue->list);
	spin_unlock_irqrestore(&md_queue->locker, flags);
	schedule_work(&md_queue->work);

	return 0;
}

uint8_t mddp_get_md_version(void)
{
	return mddp_md_version_s;
}

void mddp_set_md_version(uint8_t version)
{
	mddp_md_version_s = version;
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
	mddp_sm_uninit();
}
module_init(mddp_init);
module_exit(mddp_exit);
