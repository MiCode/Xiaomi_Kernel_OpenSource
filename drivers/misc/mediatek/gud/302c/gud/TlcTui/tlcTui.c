/*
 * Copyright (c) 2013-2014 TRUSTONIC LIMITED
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
DECLARE_COMPLETION(dci_comp);
DECLARE_COMPLETION(io_comp);

/* ------------------------------------------------------------- */
/* Static */
static const uint32_t DEVICE_ID = MC_DEVICE_ID_DEFAULT;
static struct task_struct *thread_id;
static uint32_t g_cmd_id = TLC_TUI_CMD_NONE;
static struct mc_session_handle dr_session_handle = {0, 0};
static struct tlc_tui_response_t g_user_rsp = {
	TLC_TUI_CMD_NONE, TLC_TUI_ERR_UNKNOWN_CMD};
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
	if (MC_DRV_OK != mc_ret) {
		pr_debug("ERROR %s: Allocation of DCI WSM failed: %d\n",
			 __func__, mc_ret);
		return false;
	}

	/* Clear the session handle */
	memset(&dr_session_handle, 0, sizeof(dr_session_handle));
	/* The device ID (default device is used */
	dr_session_handle.device_id = DEVICE_ID;
	/* Open session with the Driver */
	mc_ret = mc_open_session(&dr_session_handle, &dr_uuid, (uint8_t *)dci,
			(uint32_t)sizeof(struct tui_dci_msg_t));
	if (MC_DRV_OK != mc_ret) {
		pr_debug("ERROR %s: Open driver session failed: %d\n",
			 __func__, mc_ret);
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
	pr_debug("%s: Opening tbase device\n", __func__);
	mc_ret = mc_open_device(DEVICE_ID);

	/* In case the device is already open, mc_open_device will return an
	 * error (MC_DRV_ERR_INVALID_OPERATION).  But in this case, we can
	 * continue, even though mc_open_device returned an error.  Stop in all
	 * other case of error
	 */
	if (MC_DRV_OK != mc_ret && MC_DRV_ERR_INVALID_OPERATION != mc_ret) {
		pr_debug("ERROR %s: Error %d opening device\n", __func__,
			 mc_ret);
		return false;
	}

	pr_debug("%s: Opening driver session\n", __func__);
	ret = tlc_open_driver();

	return ret;
}


/* ------------------------------------------------------------- */
static void tlc_wait_cmd_from_driver(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;

	/* Wait for a command from secure driver */
	ret = mc_wait_notification(&dr_session_handle, -1);
	if (MC_DRV_OK == ret)
		pr_debug("tlc_wait_cmd_from_driver: Got a command\n");
	else
		pr_debug("ERROR %s: mc_wait_notification() failed: %d\n",
			 __func__, ret);
}


static uint32_t send_cmd_to_user(uint32_t command_id)
{
	uint32_t ret = TUI_DCI_ERR_NO_RESPONSE;

	/* Init shared variables */
	g_cmd_id = command_id;
	g_user_rsp.id = TLC_TUI_CMD_NONE;
	g_user_rsp.return_code = TLC_TUI_ERR_UNKNOWN_CMD;

	/* Give way to ioctl thread */
	complete(&dci_comp);
	pr_debug("send_cmd_to_user: give way to ioctl thread\n");

	/* Wait for ioctl thread to complete */
	wait_for_completion(&io_comp);
	pr_debug("send_cmd_to_user: Got an answer from ioctl thread.\n");
	reinit_completion(&io_comp);

	/* Check id of the cmd processed by ioctl thread (paranoia) */
	if (g_user_rsp.id != command_id) {
		pr_debug("ERROR %s: Wrong response id 0x%08x iso 0x%08x\n",
			 __func__, dci->nwd_rsp.id, RSP_ID(command_id));
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
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

	return ret;
}

/* ------------------------------------------------------------- */
static void tlc_process_cmd(void)
{
	uint32_t ret = TUI_DCI_ERR_INTERNAL_ERROR;
	uint32_t command_id = CMD_TUI_SW_NONE;

	if (NULL == dci) {
		pr_debug("ERROR %s: DCI has not been set up properly - exiting"\
			 "\n", __func__);
		return;
	} else {
		command_id = dci->cmd_nwd.id;
	}

	/* Warn if previous response was not acknowledged */
	if (CMD_TUI_SW_NONE == command_id) {
		pr_debug("ERROR %s: Notified without command\n", __func__);
		return;
	} else {
		if (dci->nwd_rsp.id != CMD_TUI_SW_NONE)
			pr_debug("%s: Warning, previous response not ack\n",
				 __func__);
	}

	/* Handle command */
	switch (command_id) {
	case CMD_TUI_SW_OPEN_SESSION:
		pr_debug("%s: CMD_TUI_SW_OPEN_SESSION.\n", __func__);

		/* Start android TUI activity */
		ret = send_cmd_to_user(TLC_TUI_CMD_START_ACTIVITY);
		if (TUI_DCI_OK != ret)
			break;

		/* allocate TUI frame buffer */
		ret = hal_tui_alloc(dci->nwd_rsp.alloc_buffer,
				dci->cmd_nwd.payload.alloc_data.alloc_size,
				dci->cmd_nwd.payload.alloc_data.num_of_buff);

		if (TUI_DCI_OK != ret)
			break;

		/* Deactivate linux UI drivers */
		ret = hal_tui_deactivate();

		if (TUI_DCI_OK != ret) {
			hal_tui_free();
			send_cmd_to_user(TLC_TUI_CMD_STOP_ACTIVITY);
			break;
		}

		break;

	case CMD_TUI_SW_CLOSE_SESSION:
		pr_debug("%s: CMD_TUI_SW_CLOSE_SESSION.\n", __func__);

		/* Activate linux UI drivers */
		ret = hal_tui_activate();

		hal_tui_free();

		/* Stop android TUI activity */
		ret = send_cmd_to_user(TLC_TUI_CMD_STOP_ACTIVITY);
		break;

	default:
		pr_debug("ERROR %s: Unknown command %d\n",
			 __func__, command_id);
		break;
	}

	/* Fill in response to SWd, fill ID LAST */
	pr_debug("%s: return 0x%08x to cmd 0x%08x\n",
		 __func__, ret, command_id);
	dci->nwd_rsp.return_code = ret;
	dci->nwd_rsp.id = RSP_ID(command_id);

	/* Acknowledge command */
	dci->cmd_nwd.id = CMD_TUI_SW_NONE;

	/* Notify SWd */
	pr_debug("DCI RSP NOTIFY CORE\n");
	ret = mc_notify(&dr_session_handle);
	if (MC_DRV_OK != ret)
		pr_debug("ERROR %s: Notify failed: %d\n", __func__, ret);
}


/* ------------------------------------------------------------- */
static void tlc_close_driver(void)
{
	enum mc_result ret;

	/* Close session with the Driver */
	ret = mc_close_session(&dr_session_handle);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR %s: Closing driver session failed: %d\n",
			 __func__, ret);
	}
}


/* ------------------------------------------------------------- */
static void tlc_close(void)
{
	enum mc_result ret;

	pr_debug("%s: Closing driver session\n", __func__);
	tlc_close_driver();

	pr_debug("%s: Closing tbase\n", __func__);
	/* Close the tbase device */
	ret = mc_close_device(DEVICE_ID);
	if (MC_DRV_OK != ret) {
		pr_debug("ERROR %s: Closing tbase device failed: %d\n",
			 __func__, ret);
	}
}

void reset_global_command_id(void)
{
	g_cmd_id = TLC_TUI_CMD_NONE;
}

/* ------------------------------------------------------------- */
bool tlc_notify_event(uint32_t event_type)
{
	bool ret = false;
	enum mc_result result;

	if (NULL == dci) {
		pr_debug("ERROR tlc_notify_event: DCI has not been set up "\
			 "properly - exiting\n");
		return false;
	}

	/* Prepare notification message in DCI */
	pr_debug("tlc_notify_event: event_type = %d\n", event_type);
	dci->nwd_notif = event_type;

	/* Signal the Driver */
	pr_debug("DCI EVENT NOTIFY CORE\n");
	result = mc_notify(&dr_session_handle);
	if (MC_DRV_OK != result) {
		pr_debug("ERROR tlc_notify_event: mc_notify failed: %d\n",
			 result);
		ret = false;
	} else {
		ret = true;
	}

	return ret;
}

/* ------------------------------------------------------------- */
/**
 */
int main_thread(void *uarg)
{
	pr_debug("main_thread: TlcTui start!\n");

	/* Open session on the driver */
	if (!tlc_open()) {
		pr_debug("ERROR main_thread: open driver failed!\n");
		return 1;
	}

	/* TlcTui main thread loop */
	for (;;) {
		/* Wait for a command from the DrTui on DCI*/
		tlc_wait_cmd_from_driver();
		/* Something has been received, process it. */
		tlc_process_cmd();
	}

	/* Close tlc. Note that this frees the DCI pointer.
	 * Do not use this pointer after tlc_close().*/
	tlc_close();

	return 0;
}

int tlc_wait_cmd(uint32_t *cmd_id)
{
	/* Create the TlcTui Main thread and start secure driver (only
	   1st time) */
	if (dr_session_handle.session_id == 0) {
		thread_id = kthread_run(main_thread, NULL, "dci_thread");
		if (!thread_id) {
			pr_debug(KERN_ERR "Unable to start Trusted UI main thread\n");
			return -EFAULT;
		}
	}

	/* Wait for signal from DCI handler */
	/* In case of an interrupted sys call, return with -EINTR */
	if (wait_for_completion_interruptible(&dci_comp)) {
		pr_debug("interrupted by system\n");
		return -ERESTARTSYS;
	}
	reinit_completion(&dci_comp);

	*cmd_id = g_cmd_id;
	return 0;
}

int tlc_ack_cmd(struct tlc_tui_response_t *rsp_id)
{
	g_user_rsp = *rsp_id;

	/* Send signal to DCI */
	complete(&io_comp);

	return 0;
}

/** @} */
