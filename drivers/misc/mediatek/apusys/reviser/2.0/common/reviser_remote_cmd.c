// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/mutex.h>

#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_remote.h"
#include <apu_ctrl_rpmsg.h>
#include "reviser_msg.h"
#include "reviser_remote_cmd.h"


int reviser_remote_check_reply(void *reply)
{
	struct reviser_msg *msg;

	if (reply == NULL) {
		LOG_ERR("Reply Null\n");
		return -EINVAL;
	}

	msg = (struct reviser_msg *)reply;
	if (msg->ack != 0) {
		LOG_ERR("Reply Ack Error %x\n", msg->ack);
		return -EINVAL;
	}

	return 0;
}
int reviser_remote_print_hw_boundary(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_HW_BOUNDARY;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}
int reviser_remote_print_hw_ctx(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_HW_CTX;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}

int reviser_remote_print_hw_rmp_table(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_HW_RMP_TABLE;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}

int reviser_remote_print_hw_default_iova(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_HW_DEFAULT_IOVA;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}
int reviser_remote_print_hw_exception(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;


	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_HW_EXCEPTION;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}

int reviser_remote_print_table_tcm(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_TABLE_TCM;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}

int reviser_remote_print_table_ctx(void *drvinfo)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_TABLE_CTX;
	req.option = REVISER_OPTION_PRINT;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}


int reviser_remote_print_table_vlm(void *drvinfo, uint32_t ctx)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;


	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_TABLE_VLM;
	req.option = REVISER_OPTION_PRINT;
	req.data[0] = ctx;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return 0;
}


int reviser_remote_set_dbg_loglevel(void *drvinfo, uint32_t level)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_DBG_LOGLEVEL;
	req.option = REVISER_OPTION_SET;

	req.data[0] = level;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}

int reviser_remote_get_dbg_loglevel(void *drvinfo, uint32_t *level)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	uint32_t value = 0;
	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));


	req.cmd = REVISER_CMD_DBG_LOGLEVEL;
	req.option = REVISER_OPTION_GET;

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
	value = reply.data[0];

	*level = value;
out:
	return ret;
}

int reviser_remote_set_op(void *drvinfo, uint32_t *argv, uint32_t argc)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	uint32_t i = 0;
	int ret = 0;
	uint32_t max_data = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	max_data = ARRAY_SIZE(req.data);
	if (argc > max_data) {
		LOG_ERR("invalid argc %d / %d\n", argc, max_data);
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	req.cmd = REVISER_CMD_DBG_OP;
	req.option = REVISER_OPTION_SET;

	for (i = 0; i < argc; i++)
		req.data[i] = argv[i];


	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}
	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}

out:
	return ret;
}

int reviser_remote_handshake(void *drvinfo, void *remote)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;

	int ret = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));


	req.cmd = REVISER_CMD_HANDSHAKE;
	req.option = REVISER_OPTION_GET;

	LOG_INFO("Remote Handshake...\n");
	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Remote Handshake Fail %d\n", ret);
		goto out;
	}

	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
	//Init Remote Info
	rdv->remote.hw_ver = reply.data[0];

	reviser_remote_sync_sn(drvinfo, reply.sn);

	LOG_INFO("Remote HW Version %x\n", rdv->remote.hw_ver);
	LOG_INFO("Done\n");
out:
	return ret;
}
