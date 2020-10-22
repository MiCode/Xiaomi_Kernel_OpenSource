// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>

#ifdef RV_COMP

#include <linux/mutex.h>

#include <apu_ipi_id.h>
#include <apu_ctrl_rpmsg.h>
#include <apu_mbox.h>

enum MDLA_DBGFS_DIR_TYPE {
	MDLA_DBGFS_READ,
	MDLA_DBGFS_WRITE,
};

/*
 * type0: Distinguish pwr_time, timeout, klog, or CX
 * type1: prepare to C1~C16
 * dir  : Distinguish read or write
 * data : store data
 */

struct mdla_dbgfs_data {
	u32 type0;
	u16 type1;
	u16 dir;
	u64 data;
};

static struct mdla_dbgfs_data ipi_cmd_compl_reply;
static struct mutex mdla_ipi_mtx;

int mdla_ipi_send(int type_0, int type_1, u64 val)
{
	struct mdla_dbgfs_data ipi_cmd_send;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.type1  = type_1;
	ipi_cmd_send.dir    = MDLA_DBGFS_WRITE;
	ipi_cmd_send.data   = val;

	mutex_lock(&mdla_ipi_mtx);
	apu_ctrl_send_msg(MDLA_SEND_CMD_COMPL_ID, &ipi_cmd_send,
				sizeof(ipi_cmd_send), 1000);
	mutex_unlock(&mdla_ipi_mtx);

	return 0;
}

int mdla_ipi_recv(int type_0, int type_1, u64 *val)
{
	struct mdla_dbgfs_data ipi_cmd_send;

	ipi_cmd_send.type0  = type_0;
	ipi_cmd_send.type1  = type_1;
	ipi_cmd_send.dir    = MDLA_DBGFS_READ;
	ipi_cmd_send.data   = 0;

	mutex_lock(&mdla_ipi_mtx);
	apu_ctrl_send_msg(MDLA_SEND_CMD_COMPL_ID, &ipi_cmd_send,
				sizeof(ipi_cmd_send), 1000);
	*val  = (u64)ipi_cmd_compl_reply.data;
	mutex_unlock(&mdla_ipi_mtx);

	return 0;
}

int mdla_ipi_init(void)
{
	mutex_init(&mdla_ipi_mtx);

	apu_ctrl_register_channel(MDLA_SEND_CMD_COMPL_ID, NULL, NULL,
				&ipi_cmd_compl_reply,
				sizeof(ipi_cmd_compl_reply));
	return 0;
}

void mdla_ipi_deinit(void)
{
	apu_ctrl_unregister_channel(MDLA_SEND_CMD_COMPL_ID);
	mutex_destroy(&mdla_ipi_mtx);
}
#else

u64 mdla_ipi_send(int type_0, int type_1, u64 val)
{
	return 0;
}

u64 mdla_ipi_recv(int type_0, int type_1, u64 *val)
{
	return 0;
}

int mdla_ipi_init(void)
{
	return 0;
}

void mdla_ipi_deinit(void)
{
}

#endif /* RV_COMP */

