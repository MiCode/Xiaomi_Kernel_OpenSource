// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>

#include "mobicore_driver_api.h"
#include "tui_ioctl.h"
#include "tlcTui.h"
#include "dciTui.h"
#include "tui-hal.h"

/* ------------------------------------------------------------- */
/* Globals */
struct tui_dci_msg_t *dci;
static DECLARE_COMPLETION(dci_comp);
static DECLARE_COMPLETION(io_comp);
static struct tlc_tui_ioctl_buffer_info buff_info;

/* ------------------------------------------------------------- */
/* Static */
static const u32 DEVICE_ID = MC_DEVICE_ID_DEFAULT;
static struct task_struct *thread_id;
static DEFINE_MUTEX(thread_mutex);
static struct tlc_tui_command_t g_user_cmd = {.id = TLC_TUI_CMD_NONE};
static struct mc_session_handle dr_session_handle = {0, 0};
struct tlc_tui_response_t g_user_rsp = {.id = TLC_TUI_CMD_NONE,
				.return_code = TLC_TUI_ERR_UNKNOWN_CMD};
static bool g_dci_version_checked;

/* Functions */

/* ------------------------------------------------------------- */
static bool tlc_open_driver(void)
{
	bool ret = false;
	enum mc_result mc_ret;
	struct mc_uuid_t dr_uuid = DR_TUI_UUID;

	/* Allocate WSM buffer for the DCI */
	mc_ret = mc_malloc_wsm(DEVICE_ID, 0, sizeof(struct tui_dci_msg_t),
			       (uint8_t **)&dci, 0);
	if (mc_ret != MC_DRV_OK) {
		tui_dev_err(mc_ret, "%d Allocation of DCI WSM failed",
			    __LINE__);
		return false;
	}

	/* Clear the session handle */
	memset(&dr_session_handle, 0, sizeof(dr_session_handle));
	/* The device ID (default device is used */
	dr_session_handle.device_id = DEVICE_ID;
	/* Open session with the Driver */
	mc_ret = mc_open_session(&dr_session_handle, &dr_uuid, (uint8_t *)dci,
				 (u32)sizeof(struct tui_dci_msg_t));
	if (mc_ret != MC_DRV_OK) {
		tui_dev_err(mc_ret, "%d mc_open_session() failed", __LINE__);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

/* ------------------------------------------------------------- */
static bool tlc_open(void)
{
	bool ret = false;
	enum mc_result mc_ret;

	/* Open the tbase device */
	tui_dev_devel("Opening TEE device");
	mc_ret = mc_open_device(DEVICE_ID);

	/* In case the device is already open, mc_open_device will return an
	 * error (MC_DRV_ERR_INVALID_OPERATION).  But in this case, we can
	 * continue, even though mc_open_device returned an error.  Stop in all
	 * other case of error
	 */
	if (MC_DRV_OK != mc_ret && MC_DRV_ERR_INVALID_OPERATION != mc_ret) {
		tui_dev_err(mc_ret, "%d mc_open_device() failed", __LINE__);
		return false;
	}

	tui_dev_devel("Opening driver session");
	ret = tlc_open_driver();

	return ret;
}

/* ------------------------------------------------------------- */
static void tlc_wait_cmd_from_driver(void)
{
	u32 ret = TUI_DCI_ERR_INTERNAL_ERROR;

	/* Wait for a command from secure driver */
	ret = mc_wait_notification(&dr_session_handle, -1);
	if (ret == MC_DRV_OK)
		tui_dev_devel("Got a command");
	else
		tui_dev_err(ret, "%d mc_wait_notification() failed", __LINE__);
}

struct mc_session_handle *get_session_handle(void)
{
	return &dr_session_handle;
}

u32 send_cmd_to_user(u32 command_id, u32 data0, u32 data1)
{
	u32 ret = TUI_DCI_ERR_NO_RESPONSE;
	int retry = 10;

	/* Init shared variables */
	g_user_cmd.id = command_id;
	g_user_cmd.data[0] = data0;
	g_user_cmd.data[1] = data1;
	/* Erase the rsp struct */
	memset(&g_user_rsp, 0, sizeof(g_user_rsp));
	g_user_rsp.id = TLC_TUI_CMD_NONE;
	g_user_rsp.return_code = TLC_TUI_ERR_UNKNOWN_CMD;

	while (!atomic_read(&fileopened) && retry--) {
		msleep(100);
		tui_dev_devel(
			"sleep for atomic_read(&fileopened) with retry = %d",
			      retry);
	}

	/*
	 * Check that the client (TuiService) is still present before to return
	 * the command.
	 */
	if (atomic_read(&fileopened)) {
		/* Clean up previous response. */
		complete_all(&io_comp);
		reinit_completion(&io_comp);

		/*
		 * Unlock the ioctl thread (IOCTL_WAIT) in order to let the
		 * client know that there is a command to process.
		 */
		tui_dev_devel("give way to ioctl thread");
		complete(&dci_comp);
		tui_dev_devel(
			"TUI TLC is running, waiting for the userland response"
			);
		/* Wait for the client acknowledge (IOCTL_ACK). */
		unsigned long completed = wait_for_completion_timeout(&io_comp,
				msecs_to_jiffies(5000));
		if (!completed) {
			tui_dev_err(-1,
				    "%d No acknowledge from client, timeout!",
				    __LINE__);
		}
	} else {
		/*
		 * There is no client, do nothing except reporting an error to
		 * SWd.
		 */
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
		tui_dev_err(ret,
			    "TUI TLC seems dead. Not waiting for userland answer");
		goto end;
	}

	tui_dev_devel("Got an answer from ioctl thread.");
	reinit_completion(&io_comp);

	/* Check id of the cmd processed by ioctl thread (paranoia) */
	if (g_user_rsp.id != command_id) {
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
		tui_dev_err(ret, "%d Wrong response id 0x%08x iso 0x%08x",
			    __LINE__,
			    dci->nwd_rsp.id,
			    (u32)RSP_ID(command_id));
	} else {
		/* retrieve return code */
		switch (g_user_rsp.return_code) {
		case TLC_TUI_OK:
			ret = TUI_DCI_OK;
			break;
		case TLC_TUI_ERROR:
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			break;
		case TLC_TUI_ERR_UNKNOWN_CMD:
			ret = TUI_DCI_ERR_UNKNOWN_CMD;
			break;
		}
	}

end:
	/*
	 * In any case, reset the value of the command, to ensure that commands
	 * sent due to inturrupted wait_for_completion are TLC_TUI_CMD_NONE.
	 */
	reset_global_command_id();
	return ret;
}

/* ------------------------------------------------------------- */
static void tlc_process_cmd(void)
{
	u32 ret = TUI_DCI_ERR_INTERNAL_ERROR;
	u32 command_id = CMD_TUI_SW_NONE;

	if (!dci) {
		tui_dev_err(-1, "%d DCI has not been set up properly - exiting",
			    __LINE__);
		return;
	}

	command_id = dci->cmd_nwd.id;

	if (dci->hal_rsp)
		hal_tui_notif();

	/* Warn if previous response was not acknowledged */
	if (command_id == CMD_TUI_SW_NONE) {
		tui_dev_err(-1, "%d Notified without command",
			    __LINE__);
		return;
	}

	if (dci->nwd_rsp.id != CMD_TUI_SW_NONE)
		tui_dev_err(-1, "Warning, previous response not ack");

	/* Handle command */
	switch (command_id) {
	case CMD_TUI_SW_OPEN_SESSION:
		tui_dev_devel("CMD_TUI_SW_OPEN_SESSION.");

		if (!g_dci_version_checked) {
			ret = TUI_DCI_ERR_INTERNAL_ERROR;
			tui_dev_err(ret, "%d DrTui version is not compatible!",
				    __LINE__);
			break;
		}

		/* Start android TUI activity */
		ret = send_cmd_to_user(
			TLC_TUI_CMD_START_ACTIVITY,
			dci->cmd_nwd.payload.alloc_data.num_of_buff,
			dci->cmd_nwd.payload.alloc_data.alloc_size);

		if (ret != TUI_DCI_OK) {
			tui_dev_err(ret, "%d send_cmd_to_user() failed",
				    __LINE__);
			break;
		}

		/* Set the global tlc_tui_ioctl_buffer_info variable using
		 * tui_alloc_data_t fiel in the dci, received from DrTui.
		 */
		set_buffer_info(dci->cmd_nwd.payload.alloc_data);

		/* Alloc work buffer separately and send it as last buffer */
		ret = hal_tui_alloc(
			dci->nwd_rsp.alloc_buffer,
			dci->cmd_nwd.payload.alloc_data.alloc_size,
			dci->cmd_nwd.payload.alloc_data.num_of_buff);

		if (ret != TUI_DCI_OK) {
			tui_dev_err(ret, "%d hal_tui_alloc() failed", __LINE__);
			send_cmd_to_user(TLC_TUI_CMD_STOP_ACTIVITY, 0, 0);
			break;
		}

		/* Deactivate linux UI drivers */
		ret = hal_tui_deactivate();

		if (ret != TUI_DCI_OK) {
			hal_tui_free();
			send_cmd_to_user(TLC_TUI_CMD_STOP_ACTIVITY, 0, 0);
			break;
		}

		break;

	case CMD_TUI_SW_GET_VERSION: {
		tui_dev_devel("CMD_TUI_SW_GET_VERSION.");
		u32 drtui_dci_version = dci->version;
		u32 tlctui_dci_version =
			TUI_DCI_VERSION(TUI_DCI_VERSION_MAJOR,
					TUI_DCI_VERSION_MINOR);
		tui_dev_info("TlcTui DCI Version (%u.%u)",
			     TUI_DCI_VERSION_GET_MAJOR(tlctui_dci_version),
			     TUI_DCI_VERSION_GET_MINOR(tlctui_dci_version));
		tui_dev_info("DrTui DCI Version (%u.%u)",
			     TUI_DCI_VERSION_GET_MAJOR(drtui_dci_version),
			     TUI_DCI_VERSION_GET_MINOR(drtui_dci_version));
		/* Write the TlcTui DCI version in the response for the SWd */
		dci->version = tlctui_dci_version;
		g_dci_version_checked = true;
		ret = TUI_DCI_OK;
		break;
	}

	case CMD_TUI_SW_HAL:
		/* TODO Always answer, even if there is a cancel!! */
		ret = hal_tui_process_cmd(&dci->cmd_nwd.payload.hal,
					  &dci->nwd_rsp.hal_rsp);
		break;

	case CMD_TUI_SW_CLOSE_SESSION:
		tui_dev_devel("CMD_TUI_SW_CLOSE_SESSION.");

		/* QC: close ion client before activating Linux UI */
		hal_tui_free();

		/* Activate linux UI drivers */
		ret = hal_tui_activate();

		/* Stop android TUI activity */
		/* Ignore return code, because an error means the TLC has been
		 * killed, which imply that the activity is stopped already.
		 */
		send_cmd_to_user(TLC_TUI_CMD_STOP_ACTIVITY, 0, 0);
		ret = TUI_DCI_OK;

		break;

	default:
		ret = TUI_DCI_ERR_UNKNOWN_CMD;
		tui_dev_err(ret, "%d Unknown command", __LINE__);
		break;
	}

	/* Fill in response to SWd, fill ID LAST */
	tui_dev_devel("return 0x%08x to cmd 0x%08x", ret, command_id);

	/* TODO: fill data fields of pDci->nwdRsp */
	dci->nwd_rsp.return_code = ret;
	dci->nwd_rsp.id = RSP_ID(command_id);

	/* Acknowledge command */
	dci->cmd_nwd.id = CMD_TUI_SW_NONE;

	/* Notify SWd */
	tui_dev_devel("DCI RSP NOTIFY CORE");
	ret = mc_notify(&dr_session_handle);
	if (ret != MC_DRV_OK)
		tui_dev_err(ret, "%d mc_notify() failed", __LINE__);
}

/* ------------------------------------------------------------- */
static void tlc_close_driver(void)
{
	enum mc_result ret;

	/* Close session with the Driver */
	ret = mc_close_session(&dr_session_handle);
	if (ret != MC_DRV_OK)
		tui_dev_err(ret, "%d mc_close_session() failed", __LINE__);
}

/* ------------------------------------------------------------- */
static void tlc_close(void)
{
	enum mc_result ret;

	tui_dev_devel("Closing driver session");
	tlc_close_driver();

	tui_dev_devel("Closing TEE");
	/* Close the tbase device */
	ret = mc_close_device(DEVICE_ID);
	if (ret != MC_DRV_OK)
		tui_dev_err(ret, "%d mc_close_device() failed", __LINE__);
}

void reset_global_command_id(void)
{
	g_user_cmd.id = TLC_TUI_CMD_NONE;
}

/* ------------------------------------------------------------- */

bool tlc_notify_event(u32 event_type)
{
	bool ret = false;
	enum mc_result result;

	if (!dci) {
		tui_dev_err(-1, "DCI has not been set up properly - exiting");
		return false;
	}

	/* Prepare notification message in DCI */
	tui_dev_devel("event_type = %d", event_type);
	dci->nwd_notif = event_type;

	/* Signal the Driver */
	tui_dev_devel("DCI EVENT NOTIFY CORE");
	result = mc_notify(&dr_session_handle);
	if (result != MC_DRV_OK) {
		tui_dev_err(result, "%d mc_notify() failed", __LINE__);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

/* ------------------------------------------------------------- */
/**
 */
static int main_thread(void *uarg)
{
	tui_dev_devel("TlcTui start!");

	/* Open session on the driver */
	if (!tlc_open())
		return 1;

	/* TlcTui main thread loop */
	for (;;) {
		/* Wait for a command from the DrTui on DCI */
		tlc_wait_cmd_from_driver();
		/* Something has been received, process it. */
		tlc_process_cmd();
	}

	/*
	 * Close tlc. Note that this frees the DCI pointer.
	 * Do not use this pointer after tlc_close().
	 */
	tlc_close();

	return 0;
}

static int start_thread_if_needed(void)
{
	int rc = 0;

	/*
	 * Create the TlcTui Main thread and start secure driver (only 1st time)
	 */
	mutex_lock(&thread_mutex);
	if (thread_id)
		/* Already started */
		goto end;

	thread_id = kthread_run(main_thread, NULL, "tee_tui");
	if (IS_ERR_OR_NULL(thread_id)) {
		rc = PTR_ERR(thread_id);
		tui_dev_err(rc, "Unable to start Trusted UI main thread");
		thread_id = NULL;
	}

end:
	mutex_unlock(&thread_mutex);
	return rc;
}

int tlc_wait_cmd(struct tlc_tui_command_t *cmd_id)
{
	int ret = start_thread_if_needed();

	if (ret)
		return ret;

	/* Wait for signal from DCI handler */
	/* In case of an interrupted sys call, return with -EINTR */
	if (wait_for_completion_interruptible(&dci_comp)) {
		ret = -ERESTARTSYS;
		tui_dev_err(ret, "interrupted by system");
		return ret;
	}
	reinit_completion(&dci_comp);

	*cmd_id = g_user_cmd;
	return 0;
}

int tlc_init_driver(void)
{
	return start_thread_if_needed();
}

int tlc_ack_cmd(struct tlc_tui_response_t *rsp)
{
	g_user_rsp = *rsp;

	if (g_user_rsp.id == TLC_TUI_CMD_ALLOC_FB)
		hal_tui_post_start(&g_user_rsp);

	/* Send signal to DCI */
	complete(&io_comp);

	return 0;
}

/* Use to set the struct tlc_tui_ioctl_buffer_info using
 * the tui_alloc_data_t field in the dci, received from DrTui.
 */
void set_buffer_info(struct tui_alloc_data_t alloc_data)
{
	tui_dev_devel("%d", __LINE__);

	buff_info.num_of_buff = alloc_data.num_of_buff;
	buff_info.size = alloc_data.alloc_size;
	buff_info.width = alloc_data.screen_width;
	buff_info.height = alloc_data.screen_height;
	buff_info.stride = alloc_data.screen_stride;
	buff_info.bits_per_pixel = alloc_data.bits_per_pixel;
}

/* Use to get the tlc_tui_ioctl_buffer_info.
 */
void get_buffer_info(struct tlc_tui_ioctl_buffer_info *buffer_info)
{
	tui_dev_devel("%d", __LINE__);

	buffer_info->num_of_buff = buff_info.num_of_buff;
	buffer_info->size = buff_info.size;
	buffer_info->width = buff_info.width;
	buffer_info->height = buff_info.height;
	buffer_info->stride = buff_info.stride;
	buffer_info->bits_per_pixel = buff_info.bits_per_pixel;
}

/** @} */
