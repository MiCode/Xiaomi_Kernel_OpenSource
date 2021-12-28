// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/mutex.h>

#include "reviser_cmn.h"
#include "reviser_drv.h"
#include "reviser_remote.h"
#include "reviser_table_mgt.h"
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
	int widx = 0;


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
	RVR_RPMSG_write(&ctx, req.data, sizeof(ctx), widx);

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
	int widx = 0;

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

	RVR_RPMSG_write(&level, req.data, sizeof(level), widx);

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
	int ridx = 0;

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
	RVR_RPMSG_RAED(&value, reply.data, sizeof(value), ridx);

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
	int ridx = 0;
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
	RVR_RPMSG_RAED(&rdv->remote.hw_ver, reply.data, sizeof(rdv->remote.hw_ver), ridx);
	RVR_RPMSG_RAED(&rdv->remote.dram_max, reply.data, sizeof(rdv->remote.dram_max), ridx);
	rdv->plat.dram_max = rdv->remote.dram_max;


	reviser_remote_sync_sn(drvinfo, reply.sn);

	LOG_INFO("Remote HW Version %x\n", rdv->remote.hw_ver);
	LOG_INFO("Done\n");
out:
	return ret;
}

int reviser_remote_set_hw_default_iova(void *drvinfo, uint32_t ctx, uint64_t iova)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	int widx = 0;

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
	req.option = REVISER_OPTION_SET;

	RVR_RPMSG_write(&ctx, req.data, sizeof(ctx), widx);
	RVR_RPMSG_write(&iova, req.data, sizeof(iova), widx);

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

int reviser_remote_alloc_mem(void *drvinfo,
		uint32_t type, uint64_t input_addr, uint32_t size,
		uint64_t *addr, uint32_t *sid)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	uint32_t ret_id = 0, mem_op = 0, out_op = 0;
	int widx = 0;
	int ridx = 0;
	uint64_t ret_addr = 0;
	uint32_t in_addr = 0, out_addr = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_ALLOC;

	in_addr = (uint32_t) input_addr;

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&type, req.data, sizeof(type), widx);
	RVR_RPMSG_write(&in_addr, req.data, sizeof(in_addr), widx);
	RVR_RPMSG_write(&size, req.data, sizeof(size), widx);

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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	RVR_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);
	RVR_RPMSG_RAED(&ret_id, reply.data, sizeof(ret_id), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

	ret_addr = (uint64_t) out_addr;

	*sid = ret_id;
	*addr = ret_addr;
out:
	return ret;
}

int reviser_remote_free_mem(void *drvinfo, uint32_t sid, uint32_t *type,
				uint64_t *addr, uint32_t *size)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	int widx = 0;
	int ridx = 0;
	uint32_t mem_op = 0, out_op = 0;
	uint32_t out_addr = 0, out_size = 0, out_type = 0;
	uint64_t ret_addr = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_FREE;

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&sid, req.data, sizeof(sid), widx);

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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	RVR_RPMSG_RAED(&out_type, reply.data, sizeof(out_type), ridx);
	RVR_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);
	RVR_RPMSG_RAED(&out_size, reply.data, sizeof(out_size), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

	ret_addr = (uint64_t) out_addr;
	*type = out_type;
	*addr = ret_addr;
	*size = out_size;

out:
	return ret;
}

int reviser_remote_map_mem(void *drvinfo,
		uint64_t session, uint32_t sid, uint64_t *addr)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	uint32_t mem_op = 0, out_op = 0;
	int widx = 0;
	int ridx = 0;
	uint32_t out_addr = 0;
	uint64_t ret_addr = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_MAP;

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&session, req.data, sizeof(session), widx);
	RVR_RPMSG_write(&sid, req.data, sizeof(sid), widx);

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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);
	RVR_RPMSG_RAED(&out_addr, reply.data, sizeof(out_addr), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}


	ret_addr = (uint64_t) out_addr;
	*addr = ret_addr;

out:
	return ret;
}

int reviser_remote_unmap_mem(void *drvinfo,
		uint64_t session, uint32_t sid)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	uint32_t mem_op = 0, out_op = 0;
	int widx = 0, ridx = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_UNMAP;

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&session, req.data, sizeof(session), widx);
	RVR_RPMSG_write(&sid, req.data, sizeof(sid), widx);

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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}


out:
	return ret;
}

int reviser_remote_import_mem(void *drvinfo, uint64_t session, uint32_t sid)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	int widx = 0, ridx = 0;
	uint32_t mem_op = 0, out_op = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_IMPORT;

	LOG_INFO("session %x free sid %x\n", session, sid);

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&session, req.data, sizeof(session), widx);
	RVR_RPMSG_write(&sid, req.data, sizeof(sid), widx);

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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}

int reviser_remote_unimport_mem(void *drvinfo, uint64_t session, uint32_t sid)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	int widx = 0, ridx = 0;
	uint32_t mem_op = 0, out_op = 0;

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

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;


	mem_op = REVISER_MEM_UNIMPORT;

	LOG_INFO("session %x free sid %x\n", session, sid);

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&session, req.data, sizeof(session), widx);
	RVR_RPMSG_write(&sid, req.data, sizeof(sid), widx);


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

	RVR_RPMSG_RAED(&out_op, reply.data, sizeof(out_op), ridx);

	if (out_op != mem_op) {
		LOG_ERR("Check OP Fail %x/%x\n", out_op, mem_op);
		ret = -EINVAL;
		goto out;
	}

out:
	return ret;
}


int reviser_remote_get_mem_info(void *drvinfo, uint32_t type)
{
	struct reviser_dev_info *rdv = NULL;
	struct reviser_msg req, reply;
	int ret = 0;
	uint32_t pool_id = 0;
	int widx = 0;
	int ridx = 0;
	uint32_t mem_op = 0;

	DEBUG_TAG;

	if (drvinfo == NULL) {
		LOG_ERR("invalid argument\n");
		return -EINVAL;
	}

	if (type >= REVSIER_POOL_MAX) {
		LOG_ERR("invalid type %d\n", type);
		return -EINVAL;
	}

	ret = reviser_table_get_pool_index(type, &pool_id);
	if (ret) {
		LOG_ERR("get pool index Fail %d\n", ret);
		goto out;
	}

	if (!reviser_is_remote()) {
		LOG_ERR("Remote Not Init\n");
		return -EINVAL;
	}

	rdv = (struct reviser_dev_info *)drvinfo;

	memset(&req, 0, sizeof(struct reviser_msg));
	memset(&reply, 0, sizeof(struct reviser_msg));

	req.cmd = REVISER_CMD_SYSTEM_RAM;
	req.option = REVISER_OPTION_SET;

	mem_op = REVISER_MEM_INFO;

	RVR_RPMSG_write(&mem_op, req.data, sizeof(mem_op), widx);
	RVR_RPMSG_write(&type, req.data, sizeof(type), widx);

	ret = reviser_remote_send_cmd_sync(drvinfo, (void *) &req, (void *) &reply, 0);
	if (ret) {
		LOG_ERR("Send Msg Fail %d\n", ret);
		goto out;
	}

	RVR_RPMSG_RAED(&rdv->remote.pool_type[pool_id],
			reply.data, sizeof(rdv->remote.pool_type[pool_id]), ridx);
	RVR_RPMSG_RAED(&rdv->remote.pool_base[pool_id],
			reply.data, sizeof(rdv->remote.pool_base[pool_id]), ridx);
	RVR_RPMSG_RAED(&rdv->remote.pool_step[pool_id],
			reply.data, sizeof(rdv->remote.pool_step[pool_id]), ridx);
	RVR_RPMSG_RAED(&rdv->remote.pool_size[pool_id],
			reply.data, sizeof(rdv->remote.pool_size[pool_id]), ridx);
	RVR_RPMSG_RAED(&rdv->remote.pool_bank_max[pool_id],
			reply.data, sizeof(rdv->remote.pool_bank_max[pool_id]), ridx);
	RVR_RPMSG_RAED(&rdv->remote.pool_addr[pool_id],
			reply.data, sizeof(rdv->remote.pool_addr[pool_id]), ridx);

	/* update platform info*/
	rdv->plat.pool_type[pool_id] = rdv->remote.pool_type[pool_id];
	rdv->plat.pool_base[pool_id] = rdv->remote.pool_base[pool_id];
	rdv->plat.pool_step[pool_id] = rdv->remote.pool_step[pool_id];
	rdv->plat.pool_size[pool_id] = rdv->remote.pool_size[pool_id];
	rdv->plat.pool_bank_max[pool_id] = rdv->remote.pool_bank_max[pool_id];
	rdv->plat.pool_addr[pool_id] = rdv->remote.pool_addr[pool_id];

	ret = reviser_remote_check_reply((void *) &reply);
	if (ret) {
		LOG_ERR("Check Msg Fail %d\n", ret);
		goto out;
	}
out:
	return ret;
}


