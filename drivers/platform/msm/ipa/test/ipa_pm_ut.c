/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/ipa.h>
#include "../ipa_v3/ipa_pm.h"
#include "../ipa_v3/ipa_i.h"
#include "ipa_ut_framework.h"
#include <linux/delay.h>

struct callback_param {
	struct completion complete;
	enum ipa_pm_cb_event evt;
};

static int ipa_pm_ut_setup(void **ppriv)
{

	IPA_UT_DBG("Start Setup\n");

	/* decrement UT vote */
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IPA_UT");

	return 0;
}

static int ipa_pm_ut_teardown(void *priv)
{
	IPA_UT_DBG("Start Teardown\n");

	/* undo UT vote */
	IPA_ACTIVE_CLIENTS_INC_SPECIAL("IPA_UT");
	return 0;
}

/* pass completion struct as the user data/callback params */
static void ipa_pm_call_back(void *user_data, enum ipa_pm_cb_event evt)
{
	struct callback_param *param;

	param = (struct callback_param *) user_data;
	param->evt = evt;

	if (evt == IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_DBG("Activate callback called\n");
		complete_all(&param->complete);
	} else if (evt == IPA_PM_REQUEST_WAKEUP) {
		IPA_UT_DBG("Request Wakeup callback called\n");
		complete_all(&param->complete);
	} else
		IPA_UT_ERR("invalid callback - callback #%d\n", evt);
}

static int clean_up(int n, ...)
{
	va_list args;
	int i, hdl, rc = 0;

	va_start(args, n);

	IPA_UT_DBG("n = %d\n", n);

	IPA_UT_DBG("Clean up Started");

	for (i = 0; i < n; i++) {
		hdl = va_arg(args, int);

		rc = ipa_pm_deactivate_sync(hdl);
		if (rc) {
			IPA_UT_ERR("fail to deactivate client - rc = %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("deactivate failed");
			return -EFAULT;
		}
		rc = ipa_pm_deregister(hdl);
		if (rc) {
			IPA_UT_ERR("fail to deregister client - rc = %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("deregister failed");
			return -EFAULT;
		}
	}
	va_end(args);
	rc = ipa_pm_destroy();
	if (rc) {
		IPA_UT_ERR("fail to destroy pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("destroy failed");
		return -EFAULT;
	}

	return 0;
}


/* test 1.1 */
static int ipa_pm_ut_single_registration(void *priv)
{
	int rc = 0;
	int hdl, vote;
	struct callback_param user_data;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params register_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};
	user_data.evt = IPA_PM_CB_EVENT_MAX;

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&register_params, &hdl);
	if (rc) {
		IPA_UT_ERR("fail to register client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deregister(hdl);
	if (rc == 0) {
		IPA_UT_ERR("deregister was not unsuccesful - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deregister was not unsuccesful");
		return -EFAULT;
	}

	rc = ipa_pm_deferred_deactivate(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deferred deactivate client - rc = %d\n"
			, rc);
		IPA_UT_TEST_FAIL_REPORT("fail to deferred deactivate client");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 0) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deregister(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deregister client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to deregister client");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc == 0) {
		IPA_UT_ERR("activate was not unsuccesful- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate was not unsuccesful");
		return -EFAULT;
	}

	rc = ipa_pm_destroy();
	if (rc) {
		IPA_UT_ERR("terminate failed - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("terminate_failed");
	}

	return 0;
}

/* test 1.1 */
static int ipa_pm_ut_double_register_activate(void *priv)
{
	int rc = 0;
	int hdl, hdl_test, vote;
	struct callback_param user_data;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params register_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};
	user_data.evt = IPA_PM_CB_EVENT_MAX;

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&register_params, &hdl);
	if (rc) {
		IPA_UT_ERR("fail to register client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&register_params, &hdl_test);
	if (rc != -EEXIST) {
		IPA_UT_ERR("registered client with same name rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("did not to fail register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to do nothing - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("do nothing failed");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc) {
		IPA_UT_ERR("fail to do nothing on 2nd activate = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to not reactivate");
		return -EFAULT;
	}

	msleep(200);

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deactivate_sync(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deactivate client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to deactivate client");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 0) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = clean_up(1, hdl);
	return rc;
}

/* test 2 */
static int ipa_pm_ut_deferred_deactivate(void *priv)
{
	int rc = 0;
	int hdl, vote;
	struct callback_param user_data;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params register_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};
	user_data.evt = IPA_PM_CB_EVENT_MAX;

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&register_params, &hdl);
	if (rc) {
		IPA_UT_ERR("fail to register client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deferred_deactivate(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client - rc = %d\n",
		rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc) {
		IPA_UT_ERR("fail to reactivate client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("reactivate client failed");
		return -EFAULT;
	}

	msleep(200);

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deactivate_sync(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deactivate_sync client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deactivate sync failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = clean_up(1, hdl);
	return rc;
}


/*test 3*/
static int ipa_pm_ut_two_clients_activate(void *priv)
{
	int rc = 0;
	int hdl_USB, hdl_WLAN, vote;
	u32 pipes;
	struct callback_param user_data_USB;
	struct callback_param user_data_WLAN;


	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data_USB
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data_WLAN
	};
	user_data_USB.evt = IPA_PM_CB_EVENT_MAX;
	user_data_WLAN.evt = IPA_PM_CB_EVENT_MAX;

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data_USB.complete);
	init_completion(&user_data_WLAN.complete);

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_associate_ipa_cons_to_client(hdl_USB, IPA_CLIENT_USB_CONS);
	if (rc) {
		IPA_UT_ERR("fail to map client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to map client");
		return -EFAULT;
	}

	rc = ipa_pm_associate_ipa_cons_to_client(hdl_WLAN,
		IPA_CLIENT_WLAN1_CONS);
	if (rc) {
		IPA_UT_ERR("fail to map client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to map client");
		return -EFAULT;
	}

	rc = ipa_pm_associate_ipa_cons_to_client(hdl_WLAN,
		IPA_CLIENT_WLAN2_CONS);
	if (rc) {
		IPA_UT_ERR("fail to map client 2 to multiplt pipes rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("fail to map client");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_USB);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work for client 1 - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work for client 2 - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data_USB.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback 1\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data_USB.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data_USB.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data_WLAN.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback 2\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data_WLAN.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data_WLAN.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	reinit_completion(&user_data_USB.complete);
	reinit_completion(&user_data_WLAN.complete);

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deferred_deactivate(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client 1 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	msleep(200);

	rc = ipa_pm_activate(hdl_USB);
	if (rc) {
		IPA_UT_ERR("no-block activate failed - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("no-block activate fail");
		return -EFAULT;
	}

	pipes = 1 << ipa_get_ep_mapping(IPA_CLIENT_USB_CONS);
	pipes |= 1 << ipa_get_ep_mapping(IPA_CLIENT_WLAN1_CONS);
	pipes |= 1 << ipa_get_ep_mapping(IPA_CLIENT_WLAN2_CONS);

	IPA_UT_DBG("pipes = %d\n", pipes);

	rc = ipa_pm_handle_suspend(pipes);

	if (!wait_for_completion_timeout(&user_data_USB.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for wakeup_callback 1\n");
		IPA_UT_TEST_FAIL_REPORT("wakeup callback not called");
		return -ETIME;
	}

	if (user_data_USB.evt != IPA_PM_REQUEST_WAKEUP) {
		IPA_UT_ERR("Callback = %d\n", user_data_USB.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data_WLAN.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for wakeup_callback 2\n");
		IPA_UT_TEST_FAIL_REPORT("wakeup callback not called");
		return -ETIME;
	}

	if (user_data_WLAN.evt != IPA_PM_REQUEST_WAKEUP) {
		IPA_UT_ERR("Callback = %d\n", user_data_WLAN.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	reinit_completion(&user_data_USB.complete);

	rc = ipa_pm_deactivate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to deactivate_sync client 1 - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to deactivate_sync");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_USB);
	if (rc) {
		IPA_UT_ERR("no-block activate failed - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("no-block activate fail");
		return -EFAULT;
	}

	pipes = 1 << ipa_get_ep_mapping(IPA_CLIENT_USB_CONS);

	rc = ipa_pm_handle_suspend(pipes);

	if (!wait_for_completion_timeout(&user_data_USB.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for wakeup_callback 1\n");
		IPA_UT_TEST_FAIL_REPORT("wakeup callback not called");
		return -ETIME;
	}

	if (user_data_USB.evt != IPA_PM_REQUEST_WAKEUP) {
		IPA_UT_ERR("Callback = %d\n", user_data_USB.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	rc = clean_up(2, hdl_USB, hdl_WLAN);
	return rc;
}

/* test 4 */
static int ipa_pm_ut_deactivate_all_deferred(void *priv)
{

	int rc = 0;
	int hdl_USB, hdl_WLAN, hdl_MODEM, vote;
	struct callback_param user_data;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params MODEM_params = {
		.name = "MODEM",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};
	user_data.evt = IPA_PM_CB_EVENT_MAX;

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rce %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_USB);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work for client 1 - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 2- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	if (!wait_for_completion_timeout(&user_data.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback 1\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_register(&MODEM_params, &hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to register client 3 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to no-block activate - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("no-block-activate failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 3) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deferred_deactivate(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client 1 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	rc = ipa_pm_deferred_deactivate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	rc = ipa_pm_deactivate_all_deferred();
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("deactivate_all_deferred failed");
		return -EINVAL;
	}

	msleep(200);
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("clock vote went below 1");
		return -EINVAL;
	}

	rc = clean_up(3, hdl_USB, hdl_WLAN, hdl_MODEM);
	return rc;
}

/* test 5 */
static int ipa_pm_ut_deactivate_after_activate(void *priv)
{

	int rc = 0;
	int hdl, vote;
	struct callback_param user_data;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rce %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&USB_params, &hdl);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work for client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	rc = ipa_pm_deferred_deactivate(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client - rc = %d\n",
		rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	msleep(200);
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}


	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work for client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		return -EFAULT;
	}

	rc = ipa_pm_deactivate_sync(hdl);
	if (rc) {
		IPA_UT_ERR("fail to deactivate sync client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deactivate sync fail");
		return -EFAULT;
	}

	msleep(200);
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = clean_up(1, hdl);
	return rc;
}

/* test 6 */
static int ipa_pm_ut_atomic_activate(void *priv)
{
	int rc = 0;
	int hdl, vote;
	struct callback_param user_data;
	spinlock_t lock;
	unsigned long flags;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params register_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
		.user_data = &user_data
	};
	user_data.evt = IPA_PM_CB_EVENT_MAX;


	spin_lock_init(&lock);

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	init_completion(&user_data.complete);

	rc = ipa_pm_register(&register_params, &hdl);
	if (rc) {
		IPA_UT_ERR("fail to register client rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	spin_lock_irqsave(&lock, flags);
	rc = ipa_pm_activate(hdl);
	if (rc != -EINPROGRESS) {
		IPA_UT_ERR("fail to queue work - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("queue activate work failed");
		spin_unlock_irqrestore(&lock, flags);
		return -EFAULT;
	}
	spin_unlock_irqrestore(&lock, flags);

	if (!wait_for_completion_timeout(&user_data.complete, HZ)) {
		IPA_UT_ERR("timeout waiting for activate_callback\n");
		IPA_UT_TEST_FAIL_REPORT("activate callback not called");
		return -ETIME;
	}

	if (user_data.evt != IPA_PM_CLIENT_ACTIVATED) {
		IPA_UT_ERR("Callback = %d\n", user_data.evt);
		IPA_UT_TEST_FAIL_REPORT("wrong callback called");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = clean_up(1, hdl);
	return rc;
}

/* test 7 */
static int ipa_pm_ut_deactivate_loop(void *priv)
{
	int rc = 0;
	int i, hdl_USB, hdl_WLAN, vote;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_USB, 1200);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 800);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 1- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	msleep(200);
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_deferred_deactivate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to deffered deactivate client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
		return -EFAULT;
	}

	for (i = 0; i < 50; i++) {
		IPA_UT_DBG("Loop iteration #%d\n", i);

		vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
		if (vote != 2) {
			IPA_UT_ERR("clock vote is at %d\n", vote);
			IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
			return -EINVAL;
		}

		rc = ipa_pm_activate(hdl_WLAN);
		if (rc) {
			IPA_UT_ERR("fail to undo deactivate for client 2");
			IPA_UT_ERR(" - rc = %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("undo deactivate failed");
			return -EFAULT;
		}

		rc = ipa_pm_deferred_deactivate(hdl_WLAN);
		if (rc) {
			IPA_UT_ERR("fail to deffered deactivate client");
			IPA_UT_ERR(" - rc = %d\n", rc);
			IPA_UT_TEST_FAIL_REPORT("deffered deactivate fail");
			return -EFAULT;
		}
	}

	msleep(200);
	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}
	rc = clean_up(2, hdl_USB, hdl_WLAN);
	return rc;

}


/*test 8*/
static int ipa_pm_ut_set_perf_profile(void *priv)
{
	int rc = 0;
	int hdl_USB, hdl_WLAN, vote, idx;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_USB, 1200);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 800);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 1- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 1) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 1200);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 3) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = clean_up(2, hdl_USB, hdl_WLAN);
	return rc;
}

/*test 9*/
static int ipa_pm_ut_group_tput(void *priv)
{
	int rc = 0;
	int hdl_USB, hdl_WLAN, hdl_MODEM, vote, idx;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_APPS,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_APPS,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params MODEM_params = {
		.name = "MODEM",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_USB, 500);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 800);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 1- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 1) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 1) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_register(&MODEM_params, &hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to register client 3 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_MODEM, 1000);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 3 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 3) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_deactivate_sync(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to deactivate client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deactivate failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = clean_up(3, hdl_USB, hdl_WLAN, hdl_MODEM);
	return rc;

}

/*test 10*/
static int ipa_pm_ut_skip_clk_vote_tput(void *priv)
{
	int rc = 0;
	int hdl_USB, hdl_WLAN, hdl_MODEM, vote, idx;

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000}
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_MODEM,
		.skip_clk_vote = 1,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params MODEM_params = {
		.name = "MODEM",
		.group = IPA_PM_GROUP_MODEM,
		.skip_clk_vote = 1,
		.callback = ipa_pm_call_back,
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_USB, 1200);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 800);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 1- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 1) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_register(&MODEM_params, &hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to register client 3 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_MODEM, 2000);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 3 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 1) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 3) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}


	rc = ipa_pm_deactivate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to deactivate client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deactivate failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 0) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	rc = clean_up(3, hdl_USB, hdl_WLAN, hdl_MODEM);
	return rc;
}

/* Test 11 */
static int ipa_pm_ut_simple_exception(void *priv)
{
	int rc = 0;
	int hdl_USB, hdl_WLAN, hdl_MODEM, vote, idx;

	struct ipa_pm_exception exceptions = {
		.usecase = "USB",
		.threshold = {1000, 1800},
	};

	struct ipa_pm_init_params init_params = {
		.threshold_size = IPA_PM_THRESHOLD_MAX,
		.default_threshold = {600, 1000},
		.exception_size = 1,
		.exceptions[0] = exceptions,
	};

	struct ipa_pm_register_params USB_params = {
		.name = "USB",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params WLAN_params = {
		.name = "WLAN",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	struct ipa_pm_register_params MODEM_params = {
		.name = "MODEM",
		.group = IPA_PM_GROUP_DEFAULT,
		.skip_clk_vote = 0,
		.callback = ipa_pm_call_back,
	};

	rc = ipa_pm_init(&init_params);
	if (rc) {
		IPA_UT_ERR("Fail to init ipa_pm - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to init params");
		return -EFAULT;
	}

	rc = ipa_pm_register(&USB_params, &hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to register client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_register(&WLAN_params, &hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to register client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_USB, 1200);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 1 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_WLAN, 2000);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to activate sync for client 1- rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("activate sync failed");
		return -EFAULT;
	}

	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 1) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_activate(hdl_WLAN);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 2 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_register(&MODEM_params, &hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to register client 3 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to register");
		return -EFAULT;
	}

	rc = ipa_pm_set_perf_profile(hdl_MODEM, 800);
	if (rc) {
		IPA_UT_ERR("fail to set tput for client 2 rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("fail to set perf profile");
		return -EFAULT;
	}

	rc = ipa_pm_activate(hdl_MODEM);
	if (rc) {
		IPA_UT_ERR("fail to activate no block for client 3 - rc = %d\n",
			rc);
		IPA_UT_TEST_FAIL_REPORT("activate no block failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 3) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	msleep(200);
	idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 3) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = ipa_pm_deactivate_sync(hdl_USB);
	if (rc) {
		IPA_UT_ERR("fail to deactivate client - rc = %d\n", rc);
		IPA_UT_TEST_FAIL_REPORT("deactivate failed");
		return -EFAULT;
	}

	vote = atomic_read(&ipa3_ctx->ipa3_active_clients.cnt);
	if (vote != 2) {
		IPA_UT_ERR("clock vote is at %d\n", vote);
		IPA_UT_TEST_FAIL_REPORT("wrong clock vote");
		return -EINVAL;
	}

	 idx = ipa3_ctx->ipa3_active_clients.bus_vote_idx;
	if (idx != 2) {
		IPA_UT_ERR("clock plan is at %d\n", idx);
		IPA_UT_TEST_FAIL_REPORT("wrong clock plan");
		return -EINVAL;
	}

	rc = clean_up(3, hdl_USB, hdl_WLAN, hdl_MODEM);
	return rc;
}

/* Suite definition block */
IPA_UT_DEFINE_SUITE_START(pm, "PM for IPA",
	ipa_pm_ut_setup, ipa_pm_ut_teardown)
{
	IPA_UT_ADD_TEST(single_registration,
		"Single Registration/Basic Functions",
		ipa_pm_ut_single_registration,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(double_register_activate,
		"double register/activate",
		ipa_pm_ut_double_register_activate,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(deferred_deactivate,
		"Deferred_deactivate",
		ipa_pm_ut_deferred_deactivate,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(two_clients_activate,
		"Activate two clients",
		ipa_pm_ut_two_clients_activate,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(deactivate_all_deferred,
		"Deactivate all deferred",
		ipa_pm_ut_deactivate_all_deferred,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(deactivate_after_activate,
		"Deactivate after activate",
		ipa_pm_ut_deactivate_after_activate,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(atomic_activate,
		"Atomic activate",
		ipa_pm_ut_atomic_activate,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(deactivate_loop,
		"Deactivate Loop",
		ipa_pm_ut_deactivate_loop,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(set_perf_profile,
		"Set perf profile",
		ipa_pm_ut_set_perf_profile,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(group_tput,
		"Group throughputs",
		ipa_pm_ut_group_tput,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(skip_clk_vote_tput,
		"Skip clock vote and tput",
		ipa_pm_ut_skip_clk_vote_tput,
		true, IPA_HW_v4_0, IPA_HW_MAX),
	IPA_UT_ADD_TEST(simple_exception,
		"throughput while passing simple exception",
		ipa_pm_ut_simple_exception,
		true, IPA_HW_v4_0, IPA_HW_MAX),
} IPA_UT_DEFINE_SUITE_END(pm);
